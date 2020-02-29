#ifndef VKUTILS_OPTIONAL_INCLUDER_H_
#define VKUTILS_OPTIONAL_INCLUDER_H_

#if __cplusplus >= 201700L
    #include <optional>
#elif defined VKUTILS_OPTIONAL_SUBSTITUTE
    #include VKUTILS_OPTIONAL_SUBSTITUTE
#else
    #error "vkutils could not find a valid std::optional implemntation. Please use c++ 17 or provide your own substitute and set it's include path as VKUTILS_OPTIONAL_SUBSTITUTE"
#endif

#endif