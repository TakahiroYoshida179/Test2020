/**
 * @file ntma_debug.h
 * @brief デバッグpritnfの提供
 * @date 2017/05/21
 * @author fumiya.ogyu
 */

#ifndef NTMOBILE_ADAPTOR_INCLUDE_NTMA_DEBUG_H_
#define NTMOBILE_ADAPTOR_INCLUDE_NTMA_DEBUG_H_

#ifdef NDEBUG
    // NDEBUGが定義されているときは何もしない
    #define LOG( level, fmt, ... ) ((void)0)
#else /* !NDEBUG */
    // NDEBUGが定義されていないときはprintfデバッグ用関数化
    #include <stdio.h>
    #define LOG( level, fmt, ... ) \
        fprintf( level, \
                  "[%s:%u] " fmt "\n", \
                  __FILE__, \
                  __LINE__, ## __VA_ARGS__ \
        )

#endif /* NDEBUG */
#endif /* NTMOBILE_ADAPTOR_INCLUDE_NTMA_DEBUG_H_ */
