#ifndef RAYM_VCRUNTIME_EXCEPTION_SHIM
#define RAYM_VCRUNTIME_EXCEPTION_SHIM
#ifdef __cplusplus
extern "C++" {
class exception {
public: virtual const char* what() const noexcept { return ""; }
};
class bad_alloc {
public: virtual const char* what() const noexcept { return ""; }
};
class bad_array_new_length {
public: virtual const char* what() const noexcept { return ""; }
};

#endif
typedef void (*terminate_handler)();
typedef void (*unexpected_handler)();
void terminate();
terminate_handler set_terminate(terminate_handler);
terminate_handler _get_terminate();
void unexpected();
unexpected_handler set_unexpected(unexpected_handler);
unexpected_handler _get_unexpected();
#ifdef __cplusplus
}
#endif
#endif
