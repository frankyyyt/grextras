// Copyright (C) by Josh Blum. See LICENSE.txt for licensing information.

#ifndef INCLUDED_GREXTRAS_MULTIPLY_CONST_HPP
#define INCLUDED_GREXTRAS_MULTIPLY_CONST_HPP

#include <grextras/config.hpp>
#include <gras/hier_block.hpp>
#include <gras/block.hpp>

namespace grextras
{

struct GREXTRAS_API MultiplyConst : virtual gras::HierBlock
{
    typedef boost::shared_ptr<MultiplyConst> sptr;

    static sptr make_fc32_fc32(const std::complex<double> &val = 0);
    static sptr make_sc32_sc32(const std::complex<double> &val = 0);
    static sptr make_sc16_sc16(const std::complex<double> &val = 0);
    static sptr make_sc8_sc8(const std::complex<double> &val = 0);
    static sptr make_f32_f32(const std::complex<double> &val = 0);
    static sptr make_s32_s32(const std::complex<double> &val = 0);
    static sptr make_s16_s16(const std::complex<double> &val = 0);
    static sptr make_s8_s8(const std::complex<double> &val = 0);

    //! Set the value from any type
    virtual void set_const(const std::complex<double> &val) = 0;

    //! Get the constant value as a complex double
    virtual std::complex<double> get_const(void) = 0;
};

class GREXTRAS_API MultiplyConstV : virtual public gras::Block
{
public:
    typedef boost::shared_ptr<MultiplyConstV> sptr;

    /*!
     * Make a multiply const from type and const value.
     * The length of value is the vector length of this block.
     */
    static sptr make_fc32_fc32(const std::vector<std::complex<float> > &vec);
    static sptr make_sc32_sc32(const std::vector<std::complex<boost::int32_t> > &vec);
    static sptr make_sc16_sc16(const std::vector<std::complex<boost::int16_t> > &vec);
    static sptr make_sc8_sc8(const std::vector<std::complex<boost::int8_t> > &vec);
    static sptr make_f32_f32(const std::vector<float> &vec);
    static sptr make_s32_s32(const std::vector<boost::int32_t> &vec);
    static sptr make_s16_s16(const std::vector<boost::int16_t> &vec);
    static sptr make_s8_s8(const std::vector<boost::int8_t> &vec);

    //! Set the value from any vector type
    template <typename type>
    void set_const(const std::vector<type> &val);

    //! Get the constant value as a vector of complex double
    virtual std::vector<std::complex<double> > get_const(void) = 0;

private:
    virtual void _set_const(const std::vector<std::complex<double> > &val) = 0;

};

}

//--- template implementation details below ---//

template <typename type>
void grextras::MultiplyConstV::set_const(const std::vector<type> &val)
{
    std::vector<std::complex<double> > new_val;
    for (size_t i = 0; i < val.size(); i++)
    {
        new_val.push_back(num_to_complex128(val[i]));
    }
    return this->_set_const(new_val);
}

#endif /*INCLUDED_GREXTRAS_MULTIPLY_CONST_HPP*/
