/*
 * Original source code from https://github.com/ximtech/Regex
 * 
 * MIT License
 * 
 * Copyright (c) 2021 ximtech
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * FORKED FROM: tiny-regex-c, see https://github.com/kokke/tiny-regex-c
 *
 * Supports:
 * ---------
 *   '.'        Dot, matches any character
 *   '^'        Start anchor, matches beginning of string
 *   '$'        End anchor, matches end of string
 *   '*'        Asterisk, match zero or more (greedy)
 *   '+'        Plus, match one or more (greedy)
 *   '*?'       Asterisk, match zero or more (lazy)
 *   '+?'       Plus, match one or more (lazy)
 *   '?'        Question, match zero or one (lazy)
 *   '{m,n}'    Quantifier, match min. 'm' and max. 'n' (greedy)
 *   '{m}'      Exactly 'm'
 *   '{m,}'     Match min 'm' and max.
 *   '[abc]'    Character class, match if one of {'a', 'b', 'c'}
 *   '[^abc]'   Inverted class, regexMatch if NOT one of {'a', 'b', 'c'}
 *   '[a-zA-Z]' Character ranges, the character set of the ranges { a-z | A-Z }
 *   '\s'       Whitespace, \t \f \r \n \v and spaces
 *   '\S'       Non-whitespace
 *   '\w'       Alphanumeric, [a-zA-Z0-9_]
 *   '\W'       Non-alphanumeric
 *   '\d'       Digits, [0-9]
 *   '\D'       Non-digits
 */

// Define to 0(false) if you DON'T want '.' to regexMatch '\r' + '\n'
#define REGEX_DOT_MATCH_NEWLINE true
#define MAX_REGEXP_OBJECTS  4    // Max number of regex symbols in expression.
#define MAX_CHAR_CLASS_LENGTH  64    // Max length of character-class buffer in. Determines the size of buffer for chars in all char-classes in the expression.
#define MAX_SUBEXPRESSIONS 20    // Max number of subexpressions when substituting with a replacement that matches part of a pattern.

typedef enum RegexPatternType {
    REGEX_END_OF_PATTERN,     // is a sentinel used to indicate end-of-pattern
    REGEX_DOT,
    REGEX_BEGIN,
    REGEX_DOLLAR_END,
    REGEX_QUESTION_MARK,
    REGEX_STAR,
    REGEX_LAZY_STAR,
    REGEX_PLUS,
    REGEX_LAZY_PLUS,
    REGEX_REGULAR_CHAR,
    REGEX_CHAR_CLASS,
    REGEX_INVERSE_CHAR_CLASS,
    REGEX_DIGIT,
    REGEX_NOT_DIGIT,
    REGEX_ALPHA,
    REGEX_NOT_ALPHA,
    REGEX_WHITESPACE,
    REGEX_NOT_WHITESPACE,
    REGEX_QUANTIFIER
} RegexPatternType;

typedef struct RegexNode {
    RegexPatternType patternType;
    union {
        unsigned char regexChar;
        unsigned char *classCharPtr;
        uint16_t minMaxQuantifiers[2];  // {m,n} quantifier
    };
} RegexNode;

typedef struct Regex {
    struct RegexNode compiledRegexArray[MAX_REGEXP_OBJECTS];
    unsigned char classCharArray[MAX_CHAR_CLASS_LENGTH];
    bool isPatternValid;
    const char *errorMessage;
} Regex;

typedef struct Matcher {
    int32_t foundAtIndex;
    int32_t matchLength;
    bool isFound;
} Matcher;

typedef struct Substitution {
    const char *pattern;
    const char *replacement;
} Substitution;

typedef uint64_t (*SubstituteFunction)(const char *haystack, const char *pattern,
    const char *replacement, bool greedy,
    char *buffer, uint64_t bufferLength,
    bool *successful, const char **errorMessage, ...);

// Compile regex string pattern to a regex
void regexCompileLength(Regex *regex, const char *pattern, size_t patternLength);
void regexCompile(Regex *regex, const char *pattern);

// Find matches of the compiled pattern inside text.
bool regexMatchMatcher(Regex *regex, const char *text, Matcher *matcher);
Matcher regexMatch(Regex *regex, const char *text);

// Substitute a matched regular expression with the provided replacement using
// the provided output buffer.
uint64_t substitute_(const char *haystack, const char *pattern,
    const char *replacement, bool greedy,
    char *buffer, uint64_t bufferLength,
    bool *successful, const char **errorMessage, ...);

// Wrapper around substitute_ that automatically provides NULLs for the
// successful and errorMessage pointers if the caller isn't interested in them.
#define substitute(haystack, pattern, replacement, greedy, buffer, bufferLength, ...) \
    substitute_(haystack, pattern, replacement, greedy, buffer, bufferLength, ##__VA_ARGS__, 0, 0)

// Substitute a regular expression that contains one or more sub-expressions
// delineated by \( and \) with a replacement that may contain references to
// the matched sub-expressions using the provided output buffer.
uint64_t substituteMatch_(const char *haystack, const char *pattern,
    const char *replacement, bool greedy,
    char *buffer, uint64_t bufferLength,
    bool *successful, const char **errorMessage, ...);

// Wrapper around substituteMatch_ that automatically provides NULLs for the
// successful and errorMessage pointers if the caller isn't interested in them.
#define substituteMatch(haystack, pattern, replacement, greedy, buffer, bufferLength, ...) \
    substituteMatch_(haystack, pattern, replacement, greedy, buffer, bufferLength, ##__VA_ARGS__, 0, 0)

// Apply successive patterns and replacements to an initial string using two
// buffers provided.  substitutions must be a NULL-terminated array.  buffer
// must be an array of (at least) two stirng buffers.  bufferLength must be
// the minimum length of the first two buffers provided.  The index of the
// final output will be stored in the finalIndex parameter.  Substitution will
// stop when NULL is reached in the substitutions array.
uint64_t substituteMultiple_(const char *haystack, Substitution *substitutions,
    bool greedy, char **buffers, uint64_t bufferLength, unsigned int *finalIndex,
    bool *successful, const char **errorMessage, SubstituteFunction substituteFunction, ...);

#define substituteMultiple(haystack, substitutions, greedy, buffers, bufferLength, finalIndex, ...) \
    substituteMultiple_(haystack, substitutions, greedy, buffers, bufferLength, finalIndex, ##__VA_ARGS__, 0, 0, 0)

#ifdef __cplusplus
} // extern "C"
#endif

