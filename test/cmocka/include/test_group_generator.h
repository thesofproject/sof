/* CMOCKA SETUP */
#define setup_alloc(ptr, type, size, offset) do {\
	if (ptr)\
		free(ptr);\
	ptr = malloc(sizeof(type) * (size) + offset);\
	if (ptr == NULL)\
		return -1;\
} while (0)

#define setup_part(result, setup_func) do {\
	result |= setup_func;\
	if (result)\
		return result;\
} while (0)

/* TEST GROUPS GENERATORS */

#define ESCAPE_TOKEN(NAME, x, y) NAME(x, y)
// get specialisation for given number of args for a variadic macro
#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, NAME, ...) NAME

#define BASE_CONCAT(x, y) x ## _ ## y // pattern of concatenisation
// pseudo variadic token concat
#define CONCAT(...) GET_MACRO(__VA_ARGS__, \
CONCAT9, CONCAT8, CONCAT7, CONCAT6, CONCAT5, \
CONCAT4, CONCAT3, CONCAT2, CONCAT1)(__VA_ARGS__)
#define CONCAT1(f1) f1
#define CONCAT2(f1, ...) ESCAPE_TOKEN(BASE_CONCAT, f1, CONCAT1(__VA_ARGS__))
#define CONCAT3(f1, ...) ESCAPE_TOKEN(BASE_CONCAT, f1, CONCAT2(__VA_ARGS__))
#define CONCAT4(f1, ...) ESCAPE_TOKEN(BASE_CONCAT, f1, CONCAT3(__VA_ARGS__))
#define CONCAT5(f1, ...) ESCAPE_TOKEN(BASE_CONCAT, f1, CONCAT4(__VA_ARGS__))
#define CONCAT6(f1, ...) ESCAPE_TOKEN(BASE_CONCAT, f1, CONCAT5(__VA_ARGS__))
#define CONCAT7(f1, ...) ESCAPE_TOKEN(BASE_CONCAT, f1, CONCAT6(__VA_ARGS__))
#define CONCAT8(f1, ...) ESCAPE_TOKEN(BASE_CONCAT, f1, CONCAT7(__VA_ARGS__))
#define CONCAT9(f1, ...) ESCAPE_TOKEN(BASE_CONCAT, f1, CONCAT8(__VA_ARGS__))

#define gen_test_concat(prefix, name) CONCAT(prefix, name)
#define gen_test_concat_base(prefix) CONCAT(prefix, base)
#define gen_test_concat_func(prefix, name, ...) \
CONCAT(prefix, name, for, __VA_ARGS__)
#define gen_test_concat_flag(prefix, ...) \
CONCAT(prefix, isetup, for, __VA_ARGS__)

// make function body for "${prefix}_${name}_for_${dlen}_${cbeg}_${cend}"
#define gen_test_with_prefix(prefix_check, prefix_setup, name, ...) \
static void gen_test_concat_func(prefix_setup, name, __VA_ARGS__) \
(void **state) {\
	if (!gen_test_concat_flag(prefix_setup, __VA_ARGS__)) {\
		if (_setup(state, __VA_ARGS__))\
			exit(1);\
		gen_test_concat_base(prefix_setup)(state);\
		++gen_test_concat_flag(prefix_setup, __VA_ARGS__);\
	} \
	gen_test_concat(prefix_check, name)(state);\
}

#define c_u_t(x) cmocka_unit_test(x)
#define c_u_t_concat(prefix_check, prefix_setup, name, ...)\
	c_u_t(gen_test_concat_func(prefix_setup, name, __VA_ARGS__)),

/* make function bodies for
 * function group "${prefix}_for_${dlen}_${cbeg}_${cend}"
 */
#define gen_test_group(bind_macro, ...)\
	bind_macro(gen_test_with_prefix, __VA_ARGS__)

/* create 1 flag for
 * function group "${prefix}_for_${dlen}_${cbeg}_${cend}"
 */
#define flg_test_group(prefix, ...)\
	static int gen_test_concat_flag(prefix, __VA_ARGS__);

/* paste instances to
 * function grup "${prefix}_for_${dlen}_${cbeg}_${cend}"
 */
#define use_test_group(bind_macro, ...)\
	bind_macro(c_u_t_concat, __VA_ARGS__)

/* for example usage, see /test/cmocka/src/lib/lib/strcheck.c
 * section TEST GROUPS GENERATORS
 */
