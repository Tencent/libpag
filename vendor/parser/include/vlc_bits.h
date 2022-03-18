#ifndef vlc_bits_h
#define vlc_bits_h

typedef struct bs_s bs_t;

typedef struct
{
    /* forward read modifier (p_start, p_end, p_fwpriv, count, pos, remain) */
    size_t (*pf_byte_forward)(bs_t *, size_t);
    size_t (*pf_byte_pos)(const bs_t *);
    size_t (*pf_byte_remain)(const bs_t *);
} bs_byte_callbacks_t;

typedef struct bs_s
{
    uint8_t *p_start;
    uint8_t *p;
    uint8_t *p_end;

    ssize_t  i_left;    /* i_count number of available bits */
    bool     b_read_only;

    bs_byte_callbacks_t cb;
    void    *p_priv;
} bs_t;

static size_t bs_impl_bytes_forward( bs_t *s, size_t i_count )
{
    if( s->p == NULL )
    {
        s->p = s->p_start;
        return 1;
    }

    if( s->p >= s->p_end )
        return 0;

    if( (size_t) (s->p_end - s->p) < i_count )
        i_count = s->p_end - s->p;
    s->p += i_count;
    return i_count;
}

static size_t bs_impl_bytes_remain( const bs_t *s )
{
    if( s->p )
        return s->p < s->p_end ? s->p_end - s->p - 1: 0;
    else
        return s->p_end - s->p_start;
}

static size_t bs_impl_bytes_pos( const bs_t *s )
{
    if( s->p )
        return s->p < s->p_end ? s->p - s->p_start + 1 : s->p - s->p_start;
    else
        return 0;
}

static inline void bs_init_custom( bs_t *s, const void *p_data, size_t i_data,
                                   const bs_byte_callbacks_t *cb, void *priv )
{
    s->p_start = (uint8_t *)p_data;
    s->p       = NULL;
    s->p_end   = s->p_start + i_data;
    s->i_left  = 0;
    s->b_read_only = true;
    s->p_priv = priv;
    s->cb = *cb;
}

static inline void bs_init( bs_t *s, const void *p_data, size_t i_data )
{
    bs_byte_callbacks_t cb = {
        bs_impl_bytes_forward,
        bs_impl_bytes_pos,
        bs_impl_bytes_remain,
    };
    bs_init_custom( s, p_data, i_data, &cb, NULL );
}

static inline void bs_write_init( bs_t *s, void *p_data, size_t i_data )
{
    bs_init( s, (const void *) p_data, i_data );
    s->b_read_only = false;
}

static inline int bs_refill( bs_t *s )
{
    if( s->i_left == 0 )
    {
       if( s->cb.pf_byte_forward( s, 1 ) != 1 )
           return -1;

       if( s->p < s->p_end )
            s->i_left = 8;
    }
    return s->i_left > 0 ? 0 : 1;
}

static inline size_t bs_remain( const bs_t *s )
{
    return 8 * s->cb.pf_byte_remain( s ) + s->i_left;
}

static inline void bs_skip( bs_t *s, size_t i_count )
{
    if( i_count == 0 )
        return;

    if( bs_refill( s ) )
        return;

    if( i_count > static_cast<size_t>(s->i_left) )
    {
        i_count -= s->i_left;
        s->i_left = 0;
        if( i_count / 8 )
            s->cb.pf_byte_forward( s, i_count / 8 );
        i_count = i_count % 8;
        if( i_count > 0 && !bs_refill( s ) )
            s->i_left = 8 - i_count;
    }
    else s->i_left -= i_count;
}

static inline uint32_t bs_read( bs_t *s, uint8_t i_count )
{
     static const uint32_t i_mask[33] =
     {  0x00,
        0x01,      0x03,      0x07,      0x0f,
        0x1f,      0x3f,      0x7f,      0xff,
        0x1ff,     0x3ff,     0x7ff,     0xfff,
        0x1fff,    0x3fff,    0x7fff,    0xffff,
        0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,
        0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,
        0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
        0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff};
    uint8_t  i_shr, i_drop = 0;
    uint32_t i_result = 0;

    if( i_count > 32 )
    {
        i_drop = i_count - 32;
        i_count = 32;
    }

    while( i_count > 0 )
    {
        if( bs_refill( s ) )
            break;

        if( s->i_left > i_count )
        {
            i_shr = s->i_left - i_count;
            /* more in the buffer than requested */
            i_result |= ( *s->p >> i_shr )&i_mask[i_count];
            s->i_left -= i_count;
            break;
        }
        else
        {
            i_shr = i_count - s->i_left;
            /* less in the buffer than requested */
           if( i_shr >= 32 )
               i_result = 0;
           else
               i_result |= (*s->p&i_mask[s->i_left]) << i_shr;
           i_count  -= s->i_left;
           s->i_left = 0;
        }
    }

    if( i_drop )
        bs_skip( s, i_drop );

    return( i_result );
}

static inline uint32_t bs_read1( bs_t *s )
{
    if( bs_refill( s ) )
        return 0;
    s->i_left--;
    return ( *s->p >> s->i_left )&0x01;
}

/* Read unsigned Exp-Golomb code */
static inline uint_fast32_t bs_read_ue( bs_t * bs )
{
    unsigned i = 0;

    while( bs_read1( bs ) == 0 && bs->p < bs->p_end && i < 31 )
        i++;

    return (1U << i) - 1 + bs_read( bs, i );
}

/* Read signed Exp-Golomb code */
static inline int_fast32_t bs_read_se( bs_t *s )
{
    uint_fast32_t val = bs_read_ue( s );

    return (val & 0x01) ? (int_fast32_t)((val + 1) / 2)
                        : -(int_fast32_t)(val / 2);
}

struct hxxx_bsfw_ep3b_ctx_s
{
    unsigned i_prev;
    size_t i_bytepos;
    size_t i_bytesize;
};

static inline uint8_t *hxxx_ep3b_to_rbsp( uint8_t *p, uint8_t *end, unsigned *pi_prev, size_t i_count )
{
    for( size_t i=0; i<i_count; i++ )
    {
        if( ++p >= end )
            return p;

        *pi_prev = (*pi_prev << 1) | (!*p);

        if( *p == 0x03 &&
           ( p + 1 ) != end ) /* Never escape sequence if no next byte */
        {
            if( (*pi_prev & 0x06) == 0x06 )
            {
                ++p;
                *pi_prev = ((*pi_prev >> 1) << 1) | (!*p);
            }
        }
    }
    return p;
}

static void hxxx_bsfw_ep3b_ctx_init( struct hxxx_bsfw_ep3b_ctx_s *ctx )
{
    ctx->i_prev = 0;
    ctx->i_bytepos = 0;
    ctx->i_bytesize = 0;
}

static size_t hxxx_ep3b_total_size( const uint8_t *p, const uint8_t *p_end )
{
    /* compute final size */
    unsigned i_prev = 0;
    size_t i = 0;
    while( p < p_end )
    {
        uint8_t *n = hxxx_ep3b_to_rbsp( (uint8_t *)p, (uint8_t *)p_end, &i_prev, 1 );
        if( n > p )
            ++i;
        p = n;
    }
    return i;
}

static size_t hxxx_bsfw_byte_forward_ep3b( bs_t *s, size_t i_count )
{
    struct hxxx_bsfw_ep3b_ctx_s *ctx = (struct hxxx_bsfw_ep3b_ctx_s *) s->p_priv;
    if( s->p == NULL )
    {
        ctx->i_bytesize = hxxx_ep3b_total_size( s->p_start, s->p_end );
        s->p = s->p_start;
        ctx->i_bytepos = 1;
        return 1;
    }

    if( s->p >= s->p_end )
        return 0;

    s->p = hxxx_ep3b_to_rbsp( s->p, s->p_end, &ctx->i_prev, i_count );
    ctx->i_bytepos += i_count;
    return i_count;
}

static size_t hxxx_bsfw_byte_pos_ep3b( const bs_t *s )
{
    struct hxxx_bsfw_ep3b_ctx_s *ctx = (struct hxxx_bsfw_ep3b_ctx_s *) s->p_priv;
    return ctx->i_bytepos;
}

static size_t hxxx_bsfw_byte_remain_ep3b( const bs_t *s )
{
    struct hxxx_bsfw_ep3b_ctx_s *ctx = (struct hxxx_bsfw_ep3b_ctx_s *) s->p_priv;
    if( ctx->i_bytesize == 0 && s->p_start != s->p_end )
        ctx->i_bytesize = hxxx_ep3b_total_size( s->p_start, s->p_end );
    return (ctx->i_bytesize > ctx->i_bytepos) ? ctx->i_bytesize - ctx->i_bytepos : 0;
}

static const bs_byte_callbacks_t hxxx_bsfw_ep3b_callbacks =
{
    hxxx_bsfw_byte_forward_ep3b,
    hxxx_bsfw_byte_pos_ep3b,
    hxxx_bsfw_byte_remain_ep3b,
};

#endif /* vlc_bits_h */
