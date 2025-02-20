
#include <gmock/gmock.h>

#include <glbinding/gl/gl.h>

#include <glbinding/SharedBitfield.h>

using namespace glbinding;

enum class A
{
    A_a,
    A_b,
    A_c
};

A operator|(const A & a, const A & b)
{
    return static_cast<A>(static_cast<std::underlying_type<A>::type>(a) & static_cast<std::underlying_type<A>::type>(b));
}

A & operator|=(A & a, const A & b)
{
    a = static_cast<A>(static_cast<std::underlying_type<A>::type>(a) & static_cast<std::underlying_type<A>::type>(b));

    return a;
}

enum class B
{
    B_a,
    B_b,
    B_c
};

enum class C
{
    C_a,
    C_b,
    C_c
};

TEST(SharedBitfield, Usage1a)
{
    A bitsA_a = A::A_a;
    A bitsA_b = A::A_b;

    A bitsA_c = bitsA_a | bitsA_b;

    bitsA_c |= bitsA_b;

    SharedBitfield<A, B, C> shared1 = bitsA_a;
    SharedBitfield<A, B, C> shared2 = bitsA_b;

    shared1 |= shared2;

    bitsA_c |= shared1;
}

TEST(SharedBitfield, Usage1b)
{
    gl::ClearBufferMask bitsA_a = gl::ClearBufferMask::GL_ACCUM_BUFFER_BIT;
    gl::ClearBufferMask bitsA_b = gl::ClearBufferMask::GL_DEPTH_BUFFER_BIT;

    gl::ClearBufferMask bitsA_c = bitsA_a | bitsA_b;

    bitsA_c |= bitsA_b;

    SharedBitfield<gl::ClearBufferMask, gl::AttribMask> shared1 = bitsA_a;
    SharedBitfield<gl::ClearBufferMask, gl::AttribMask> shared2 = bitsA_b;

    shared1 |= shared2;

    bitsA_c |= shared1;
}

TEST(SharedBitfield, Usage2)
{
    SharedBitfield<A, B, C> bitsA_a = A::A_a;
    SharedBitfield<A, B, C> bitsA_b = A::A_b;
    SharedBitfield<A, B, C> bitsA_c = A::A_c;
    SharedBitfield<A, B, C> bitsB_a = B::B_a;
    SharedBitfield<A, B, C> bitsB_b = B::B_b;
    SharedBitfield<A, B, C> bitsB_c = B::B_c;
    SharedBitfield<A, B, C> bitsC_a = C::C_a;
    SharedBitfield<A, B, C> bitsC_b = C::C_b;
    SharedBitfield<A, B, C> bitsC_c = C::C_c;

    EXPECT_EQ(bitsA_a, A::A_a);
    EXPECT_EQ(bitsB_a, C::C_a);
    EXPECT_EQ(bitsA_b, C::C_b);
    EXPECT_EQ(bitsB_b, B::B_b);
    EXPECT_EQ(bitsA_c, A::A_c);
    EXPECT_EQ(bitsB_c, C::C_c);

    EXPECT_EQ(A::A_a, bitsA_a);
    EXPECT_EQ(C::C_a, bitsB_a);
    EXPECT_EQ(C::C_b, bitsA_b);
    EXPECT_EQ(B::B_b, bitsB_b);
    EXPECT_EQ(A::A_c, bitsA_c);
    EXPECT_EQ(C::C_c, bitsB_c);

    EXPECT_EQ(bitsA_a, bitsB_a);
    EXPECT_EQ(bitsB_a, bitsC_a);
    EXPECT_EQ(bitsA_b, bitsB_b);
    EXPECT_EQ(bitsB_b, bitsC_b);
    EXPECT_EQ(bitsA_c, bitsB_c);
    EXPECT_EQ(bitsB_c, bitsC_c);

    bitsC_c = bitsC_a;
    bitsC_b = B::B_b;
}
