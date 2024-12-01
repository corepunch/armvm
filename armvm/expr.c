#include <stdio.h>
#include <stdlib.h>

int evaluate(char** expression) {
    int result = 0;
    if (**expression == '(') {
        *expression = ++(*expression);
        return evaluate(expression);
    }
    int currentNumber = (int)strtol(*expression, expression, 10);
    int op = **expression;
    *expression = ++(*expression);
    
    switch (op) {
        case '\0':
        case ')':
            return currentNumber;
        case '+':
            return currentNumber + evaluate(expression);
        case '-':
            return currentNumber - evaluate(expression);
    }
    
    return result;
}
