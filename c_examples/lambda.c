#include <stdio.h>

void test_func(void (*lambda_test)(int, int))
{
    printf("test_func!\n");
    lambda_test(4, 4);
}

int main()
{
    test_func(({
                void f(int a, int b) 
                {
                    printf("%d + %d = %d\n", a, b, a+b);
                }f;
                }));
    return 0;
}
