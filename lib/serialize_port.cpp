// Copyright (C) by Josh Blum. See LICENSE.txt for licensing information.

#include "serialize_common.hpp"
#include <PMC/Serialize.hpp>
#include <grextras/serialize_port.hpp>
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <boost/asio.hpp> //gets me htonl
#include <boost/archive/polymorphic_text_oarchive.hpp>
#include <boost/assert.hpp>
#include <sstream>

using namespace grextras;

static gras::SBuffer pmc_to_buffer(const size_t offset_words32, const PMCC &pmc)
{
    //serialize the pmc into a stringstream
    std::ostringstream ss;
    boost::archive::polymorphic_text_oarchive oa(ss);
    try
    {
        oa << pmc;
    }
    catch(...)
    {
        std::cerr << "cannot serialize " << pmc << std::endl;
        PMCC null_pmc;
        oa << null_pmc; //null it is!
    }
    const std::string s = ss.str();
    const size_t s_words32 = (s.length() + 3)/4;

    //memcpy the stringstream into a buffer
    gras::SBufferConfig config;
    config.length = s_words32*4 + PAD_BYTES; //string length + padding
    gras::SBuffer buff(config);
    buff.length = s_words32*4;
    buff.offset = offset_words32*4;
    std::memcpy(buff.get(), (const void *)(s.c_str()), buff.length);
    return buff;
}

static void pack_buffer(const size_t seq, const size_t sid, const bool has_tsf, const gras::item_index_t tsf, const bool is_ext, gras::SBuffer &buff)
{
    BOOST_VERIFY(buff.length > 0);
    const size_t hdr_words32 = has_tsf? 6 : 4;
    BOOST_VERIFY(buff.offset*4 == hdr_words32);
    const size_t pkt_words32 = hdr_words32 + buff.length/4 + 1;
    const size_t vita_words32 = pkt_words32 - 3;
    boost::uint32_t *p = (boost::uint32_t *)buff.get_actual_memory();
    p[0] = htonl(VRLP);
    p[1] = htonl(((seq << 20) & 0xfff) | (pkt_words32 & 0xfffff));
    p[2] = htonl(VITA_SID | (is_ext? VITA_EXT : 0) | (has_tsf? VITA_TSF : 0) | ((seq << 16) & 0xf) | (vita_words32 & 0xffff));
    p[3] = htonl(sid);
    if (has_tsf) p[4] = htonl(tsf >> 32);
    if (has_tsf) p[5] = htonl(tsf >> 0);
    p[pkt_words32-1] = htonl(VEND);

    //adjust offset/length for full packet
    buff.offset = 0;
    buff.length = pkt_words32*4;
}

static gras::PacketMsg serialize_tag(const size_t seq, const size_t sid, const gras::Tag &tag)
{
    const size_t hdr_words32 = 6;
    gras::SBuffer buff = pmc_to_buffer(hdr_words32, tag.object);
    pack_buffer(seq, sid, true, tag.offset, true, buff);
    return gras::PacketMsg(buff);
}

static gras::PacketMsg serialize_msg(const size_t seq, const size_t sid, const PMCC &pmc)
{
    const size_t hdr_words32 = 4;
    gras::SBuffer buff = pmc_to_buffer(hdr_words32, pmc);
    pack_buffer(seq, sid, false, 0, true, buff);
    return gras::PacketMsg(buff);
}

static gras::PacketMsg serialize_buff(const size_t seq, const size_t sid, const void *inbuff, const size_t num_words32, gras::SBuffer &buff)
{
    const size_t hdr_words32 = 4;
    buff.length = num_words32*4;
    buff.offset = hdr_words32*4;
    std::memcpy(buff.get(), inbuff, buff.length);
    pack_buffer(seq, sid, false, 0, false, buff);
    return gras::PacketMsg(buff);
}

struct SerializePortImpl : SerializePort
{
    SerializePortImpl(const size_t mtu):
        gras::Block("GrExtras SerializePort")
    {
        this->output_config(0).reserve_items = mtu? mtu : 1400;
    }

    void work(const InputItems &ins, const OutputItems &)
    {
        for (size_t i = 0; i < ins.size(); i++)
        {
            PMCC msg = pop_input_msg(i);
            if (msg)
            {
                this->post_output_msg(0, PMC_M(serialize_msg(_seqs[i]++, i, msg)));
            }
            if (ins[i].size())
            {
                //grab output buffer
                //use the output buffer so we have to allocate
                gras::SBuffer buff = this->get_output_buffer(0);
                const size_t mtu = buff.get_actual_length() - PAD_BYTES;

                //num words calculation
                const size_t item_size = this->input_config(i).item_size;
                const size_t mtu_items = (mtu*item_size)/item_size;
                const size_t num_items = std::min(mtu_items, ins[i].size());
                const size_t num_words32 = num_items*item_size/4;

                //pack and send output msg
                const void *ptr = ins[i].cast<const void *>();
                this->serialize_tags(i, this->get_consumed(i) + num_items); //must occur before post
                this->post_output_msg(0, PMC_M(serialize_buff(_seqs[i]++, i, ptr, num_words32, buff)));
                this->consume(i, num_items);

                //we used up the output buffer -- leave now
                return;
            }
        }
    }

    void serialize_tags(const size_t i, const gras::item_index_t max_index)
    {
        BOOST_FOREACH(const gras::Tag &tag, this->get_input_tags(i))
        {
            if (tag.offset < max_index)
            {
                this->post_output_msg(0, PMC_M(serialize_tag(_seqs[i]++, i, tag)));
            }
        }
    }

    void propagate_tags(const size_t, const gras::TagIter &)
    {
        //do not propagate
    }

    void notify_topology(const size_t num_inputs, const size_t)
    {
        _seqs.resize(num_inputs, 0);
        for (size_t i = 0; i < num_inputs; i++)
        {
            //set the reserve_items to zero:
            //work gets called when not all inputs are fed
            this->input_config(i).reserve_items = 0;

            //only done when all inputs are done
            this->input_config(i).force_done = false;
        }
    }

    std::vector<size_t> _seqs;
};

SerializePort::sptr SerializePort::make(const size_t mtu)
{
    return boost::make_shared<SerializePortImpl>(mtu);
}
