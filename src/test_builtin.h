/*
 * shelli - Educational Shell
 * test_builtin.h - test / [ builtin interface
 */

#ifndef TEST_BUILTIN_H
#define TEST_BUILTIN_H

/* Execute test expression. argv[0] is "test" or "[".
 * Returns 0 for true, 1 for false, 2 for error. */
int test_builtin_execute(int argc, char **argv);

#endif /* TEST_BUILTIN_H */
