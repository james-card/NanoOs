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

#include "Regex.h"

#define END_LINE '\0'
#define MAX_QUANTIFICATION_VALUE  1024  // Max in {M,N} - Denotes the minimum M and the maximum N regexMatch count.

// Utility define.
#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

typedef struct InnerRegexCompiler {     // The sizes of the two static arrays below substantiates the static RAM usage of this module.
    Regex *regex;
    uint16_t regexIndex;
    uint16_t patternIndex;
    uint16_t classCharIndex;
    bool isQuantifiable;    // is the last node quantifiable
} RegexCompiler;

static inline void setBeginMetaChar(RegexCompiler *regexCompiler);
static inline void setDollarEndMetaChar(RegexCompiler *regexCompiler);
static inline void setDotMetaChar(RegexCompiler *regexCompiler);
static inline void setStarMetaChar(RegexCompiler *regexCompiler, const char *pattern);
static inline void setPlusMetaChar(RegexCompiler *regexCompiler, const char *pattern);
static inline void setQuestionMarkMetaChar(RegexCompiler *regexCompiler);
static inline void setRegexPatternType(RegexPatternType patternType, RegexCompiler *regexCompiler);
static void resolveEscapedCharacterClasses(RegexCompiler *regexCompiler, const char *pattern);
static void resolveCharacterClass(RegexCompiler *regexCompiler, const char *pattern);
static void resolveQuantification(RegexCompiler *regexCompiler, const char *pattern);
static inline void setRegularChar(RegexCompiler *regexCompiler, char charInPattern);

static bool matchPattern(RegexNode *regex, Matcher *matcher, const char *text);
static bool matchQuestionMark(RegexNode *regex, RegexNode *pattern, const char *text, Matcher *matcher);
static bool matchQuantifier(RegexNode *regex, RegexNode *pattern, const char *text, Matcher *matcher);

static bool matchStar(RegexNode *regex, RegexNode *pattern, const char *text, Matcher *matcher);
static bool matchStarLazy(RegexNode *regex, RegexNode *pattern, const char *text, Matcher *matcher);

static bool matchPlus(RegexNode *regex, RegexNode *pattern, const char *text, Matcher *matcher);
static bool matchPlusLazy(RegexNode *regex, RegexNode *pattern, const char *text, Matcher *matcher);

static bool matchOne(RegexNode *regex, char character);
static inline bool isMatchingDot(unsigned char character);
static bool matchCharClass(unsigned char character, const unsigned char *metaCharString);
static inline bool isMatchingRange(unsigned char character, const unsigned char *string);
static bool isMatchingMetaChar(unsigned char character, const unsigned char *metaCharString);


static inline void setCompilerChar(RegexCompiler *regexCompiler, const char *pattern, char charInPattern) {
    switch (charInPattern) {
        case '^':   // Meta-characters
            setBeginMetaChar(regexCompiler);
            break;
        case '$':
            setDollarEndMetaChar(regexCompiler);
            break;
        case '.':
            setDotMetaChar(regexCompiler);
            break;
        case '*':
            setStarMetaChar(regexCompiler, pattern);
            break;
        case '+':
            setPlusMetaChar(regexCompiler, pattern);
            break;
        case '?':
            setQuestionMarkMetaChar(regexCompiler);
            break;
        case '\\':  // Escaped characters
            resolveEscapedCharacterClasses(regexCompiler, pattern);
            break;
        case '[':   // Character class
            resolveCharacterClass(regexCompiler, pattern);
            break;
        case '{':   // Quantifier
            resolveQuantification(regexCompiler, pattern);
            break;
        default:    // Regular characters
            setRegularChar(regexCompiler, charInPattern);
    }
}

void regexCompileLength(Regex *regex, const char *pattern, size_t patternLength) {
    if (regex == NULL) return;
    regex->isPatternValid = true;
    regex->errorMessage = "Success";
    RegexCompiler regexCompiler;
    regexCompiler.classCharIndex = 0;
    regexCompiler.patternIndex = 0;
    regexCompiler.regexIndex = 0;
    regexCompiler.isQuantifiable = false;
    regexCompiler.regex = regex;

    if (pattern == NULL) {
        regex->isPatternValid = false;
        regex->errorMessage = "NULL pattern string";
        return;
    } else if (pattern[0] == END_LINE) {
        regex->isPatternValid = false;
        regex->errorMessage = "Empty pattern string";
        return;
    }

    if (patternLength == 0) {
        // Go until the end of the line.
        for (regexCompiler.patternIndex = 0;
            (pattern[regexCompiler.patternIndex] != END_LINE)
                && ((regexCompiler.regexIndex + 1) < MAX_REGEXP_OBJECTS);
            regexCompiler.patternIndex++
        ) {
            char charInPattern = pattern[regexCompiler.patternIndex];
            setCompilerChar(&regexCompiler, pattern, charInPattern);

            if (!regex->isPatternValid) {
                return;
            }
            regexCompiler.regexIndex++;
        }
    } else {
        // Process the number of characters specified.
        for (regexCompiler.patternIndex = 0;
            (regexCompiler.patternIndex < patternLength)
                && ((regexCompiler.regexIndex + 1) < MAX_REGEXP_OBJECTS);
            regexCompiler.patternIndex++
        ) {
            char charInPattern = pattern[regexCompiler.patternIndex];
            setCompilerChar(&regexCompiler, pattern, charInPattern);

            if (!regex->isPatternValid) {
                return;
            }
            regexCompiler.regexIndex++;
        }
    }

    setRegexPatternType(REGEX_END_OF_PATTERN, &regexCompiler);
}

void regexCompile(Regex *regex, const char *pattern) {
    regexCompileLength(regex, pattern, 0);
}

bool regexMatchMatcher(Regex *regex, const char *text, Matcher *matcher) {
    if (matcher == NULL) {
        // Nothing more we can do.
        return false;
    }

    matcher->foundAtIndex = 0;
    matcher->matchLength = 0;
    matcher->isFound = false;
    if ((regex == NULL) || (!regex->isPatternValid) || (text == NULL)) {
        return matcher->isFound;
    }

    if (regex->compiledRegexArray[0].patternType == REGEX_BEGIN) {
        matcher->isFound = matchPattern(regex->compiledRegexArray + 1, matcher, text);
        return matcher->isFound;
    }

    do {
        if (matchPattern(regex->compiledRegexArray, matcher, text)) {
            if (*text == END_LINE) {
                matcher->isFound = false;
                return matcher->isFound;
            }
            matcher->isFound = true;
            return matcher->isFound;
        }
        matcher->foundAtIndex++;
    } while (*text++ != END_LINE);

    return matcher->isFound;
}

Matcher regexMatch(Regex *regex, const char *text) {
    Matcher matcher;
    regexMatchMatcher(regex, text, &matcher);
    return matcher;
}

static inline void setBeginMetaChar(RegexCompiler *regexCompiler) {
    regexCompiler->isQuantifiable = false;
    setRegexPatternType(REGEX_BEGIN, regexCompiler);
}

static inline void setDollarEndMetaChar(RegexCompiler *regexCompiler) {
    regexCompiler->isQuantifiable = false;
    setRegexPatternType(REGEX_DOLLAR_END, regexCompiler);
}

static inline void setDotMetaChar(RegexCompiler *regexCompiler) {
    regexCompiler->isQuantifiable = true;
    setRegexPatternType(REGEX_DOT, regexCompiler);
}

static inline void setStarMetaChar(RegexCompiler *regexCompiler, const char *pattern) {
    if (!regexCompiler->isQuantifiable) {
        regexCompiler->regex->isPatternValid = false;
        regexCompiler->regex->errorMessage = "Non-quantifiable before '*'";
        return;
    }
    regexCompiler->isQuantifiable = false;

    if (pattern[regexCompiler->patternIndex + 1] == '?') {
        setRegexPatternType(REGEX_LAZY_STAR, regexCompiler);
        regexCompiler->patternIndex++;
    } else {
        setRegexPatternType(REGEX_STAR, regexCompiler);
    }
}

static inline void setPlusMetaChar(RegexCompiler *regexCompiler, const char *pattern) {
    if (!regexCompiler->isQuantifiable) {
        regexCompiler->regex->isPatternValid = false;
        regexCompiler->regex->errorMessage = "Non-quantifiable before '+'";
        return;
    }

    if (pattern[regexCompiler->patternIndex + 1] == '?') {
        setRegexPatternType(REGEX_LAZY_PLUS, regexCompiler);
        regexCompiler->patternIndex++;
    } else {
        setRegexPatternType(REGEX_PLUS, regexCompiler);
    }
}

static inline void setQuestionMarkMetaChar(RegexCompiler *regexCompiler) {
    if (!regexCompiler->isQuantifiable) {
        regexCompiler->regex->isPatternValid = false;
        regexCompiler->regex->errorMessage = "Non-quantifiable before '?'";
        return;
    }
    setRegexPatternType(REGEX_QUESTION_MARK, regexCompiler);
}

static inline void setRegexPatternType(RegexPatternType patternType, RegexCompiler *regexCompiler) {
    regexCompiler->regex->compiledRegexArray[regexCompiler->regexIndex].patternType = patternType;
}

static void resolveEscapedCharacterClasses(RegexCompiler *regexCompiler, const char *pattern) {
    if (pattern[regexCompiler->patternIndex + 1] != END_LINE) {
        regexCompiler->isQuantifiable = true;
        regexCompiler->patternIndex++;   // Skip the escape-char

        RegexPatternType patternType;
        switch (pattern[regexCompiler->patternIndex]) {
            case 's':
                patternType = REGEX_WHITESPACE;
                break;
            case 'S':
                patternType = REGEX_NOT_WHITESPACE;
                break;
            case 'w':
                patternType = REGEX_ALPHA;
                break;
            case 'W':
                patternType = REGEX_NOT_ALPHA;
                break;
            case 'd':
                patternType = REGEX_DIGIT;
                break;
            case 'D':
                patternType = REGEX_NOT_DIGIT;
                break;
            default:
                patternType = REGEX_END_OF_PATTERN;
                break;
        }

        if (patternType > 0) {  // Check the next Meta-character:
            setRegexPatternType(patternType, regexCompiler);
        } else {
            setRegexPatternType(REGEX_REGULAR_CHAR, regexCompiler); // Escaped character, e.g. '.' or '$'
            regexCompiler->regex->compiledRegexArray[regexCompiler->regexIndex].regexChar = pattern[regexCompiler->patternIndex];
        }
    }
}

static void resolveCharacterClass(RegexCompiler *regexCompiler, const char *pattern) {
    uint16_t bufferBegin = regexCompiler->classCharIndex;    // Remember where the char-buffer starts.
    regexCompiler->patternIndex++;  // Skip '['
    regexCompiler->isQuantifiable = true;

    if (pattern[regexCompiler->patternIndex] == '^') {    // Look-ahead to determine if negated
        setRegexPatternType(REGEX_INVERSE_CHAR_CLASS, regexCompiler);
        regexCompiler->patternIndex++; // Increment index to avoid including '^' in the char-buffer
        if (pattern[regexCompiler->patternIndex] == END_LINE) {
            regexCompiler->regex->isPatternValid = false;
            regexCompiler->regex->errorMessage = "Incomplete pattern, missing non-zero char after '^'";
            return;
        }
    } else {
        setRegexPatternType(REGEX_CHAR_CLASS, regexCompiler);
    }

    while (pattern[regexCompiler->patternIndex] != END_LINE && pattern[regexCompiler->patternIndex] != ']') {
        char charInPattern = pattern[regexCompiler->patternIndex];

        if (charInPattern == '\\') {
            if (regexCompiler->classCharIndex >= MAX_CHAR_CLASS_LENGTH - 1 || pattern[regexCompiler->patternIndex + 1] == END_LINE) {
                regexCompiler->regex->isPatternValid = false;
                regexCompiler->regex->errorMessage = "Incomplete pattern, missing non-zero char after '\\'";
                return;
            }
            regexCompiler->regex->classCharArray[regexCompiler->classCharIndex] = pattern[regexCompiler->patternIndex];
            regexCompiler->classCharIndex++;
            regexCompiler->patternIndex++;

        } else if (regexCompiler->classCharIndex >= MAX_CHAR_CLASS_LENGTH) {
            regexCompiler->regex->isPatternValid = false;
            regexCompiler->regex->errorMessage = "Exceeded internal buffer";
            return;
        }
        regexCompiler->regex->classCharArray[regexCompiler->classCharIndex] = pattern[regexCompiler->patternIndex++];
        regexCompiler->classCharIndex++;
    }

    if (regexCompiler->classCharIndex >= MAX_CHAR_CLASS_LENGTH) { // Check for too long patterns. Such as [00000000000000000000000000000000000000][
        regexCompiler->regex->isPatternValid = false;
        regexCompiler->regex->errorMessage = "Too long char class pattern";
        return;
    }

    if (pattern[regexCompiler->patternIndex] != ']') {
        regexCompiler->regex->isPatternValid = false;
        regexCompiler->regex->errorMessage = "Non terminated class ']'";
        return;
    }
    regexCompiler->regex->classCharArray[regexCompiler->classCharIndex] = END_LINE;// Null-terminate string end
    regexCompiler->regex->compiledRegexArray[regexCompiler->regexIndex].classCharPtr = &regexCompiler->regex->classCharArray[bufferBegin];
    regexCompiler->classCharIndex++;
}

static void resolveQuantification(RegexCompiler *regexCompiler, const char *pattern) {
    if (!regexCompiler->isQuantifiable) {
        regexCompiler->regex->isPatternValid = false;
        regexCompiler->regex->errorMessage = "Non-quantifiable before '{m,n}'";
        return;
    }
    regexCompiler->patternIndex++;  // Skip '{'

    if (pattern[regexCompiler->patternIndex] == END_LINE) {
        regexCompiler->regex->isPatternValid = false;
        regexCompiler->regex->errorMessage = "Dangling '{' quantifier";
        return;
    }

    uint32_t minQuantifierValue = 0;
    do
    {
        char quantifierValueChar = pattern[regexCompiler->patternIndex];
        if (!isdigit(quantifierValueChar)) {
            regexCompiler->regex->isPatternValid = false;
            regexCompiler->regex->errorMessage = "Non-digit min value in quantifier";
            return;
        }
        minQuantifierValue = 10 * minQuantifierValue + (unsigned) (quantifierValueChar - '0');
        regexCompiler->patternIndex++;
    }
    while (pattern[regexCompiler->patternIndex] != ',' && pattern[regexCompiler->patternIndex] != '}');

    if (minQuantifierValue > MAX_QUANTIFICATION_VALUE) {
        regexCompiler->regex->isPatternValid = false;
        regexCompiler->regex->errorMessage = "Min value too big in quantifier";
        return;
    }
    regexCompiler->regex->compiledRegexArray[regexCompiler->regexIndex].minMaxQuantifiers[0] = minQuantifierValue;

    if (pattern[regexCompiler->patternIndex] == ',') {
        regexCompiler->patternIndex++;  // Skip ','
        if (pattern[regexCompiler->patternIndex] == END_LINE) {
            regexCompiler->regex->isPatternValid = false;
            regexCompiler->regex->errorMessage = "Dangling ',' quantifier";
            return;
        }

        if (pattern[regexCompiler->patternIndex] == '}') {
            regexCompiler->regex->compiledRegexArray[regexCompiler->regexIndex].minMaxQuantifiers[1] = MAX_QUANTIFICATION_VALUE;
        } else {

            uint32_t maxQuantifierValue = 0;
            while (pattern[regexCompiler->patternIndex] != '}') {
                char quantifierValueChar = pattern[regexCompiler->patternIndex];
                if (quantifierValueChar == END_LINE || !isdigit(quantifierValueChar)) {
                    regexCompiler->regex->isPatternValid = false;
                    regexCompiler->regex->errorMessage = "Non-digit max value in quantifier";
                    return;
                }

                maxQuantifierValue = 10 * maxQuantifierValue + (unsigned) (quantifierValueChar - '0');
                regexCompiler->patternIndex++;
            }

            if (maxQuantifierValue > MAX_QUANTIFICATION_VALUE || maxQuantifierValue < minQuantifierValue) {
                regexCompiler->regex->isPatternValid = false;
                regexCompiler->regex->errorMessage = "Max value too big or less than min value in quantifier";
                return;
            }
            regexCompiler->regex->compiledRegexArray[regexCompiler->regexIndex].minMaxQuantifiers[1] = maxQuantifierValue;
        }
    }

    setRegexPatternType(REGEX_QUANTIFIER, regexCompiler);
}

static inline void setRegularChar(RegexCompiler *regexCompiler, char charInPattern) {
    regexCompiler->isQuantifiable = true;
    setRegexPatternType(REGEX_REGULAR_CHAR, regexCompiler);
    regexCompiler->regex->compiledRegexArray[regexCompiler->regexIndex].regexChar = charInPattern;
}

static bool matchPattern(RegexNode *regex, Matcher *matcher, const char *text) {
    int32_t previousMatch = matcher->matchLength;

    do {
        if (regex[0].patternType == REGEX_END_OF_PATTERN || regex[1].patternType == REGEX_QUESTION_MARK) {
            return matchQuestionMark(regex, (regex + 2), text, matcher);

        } else if (regex[1].patternType == REGEX_QUANTIFIER) {
            return matchQuantifier(regex, (regex + 1), text, matcher);

        } else if (regex[1].patternType == REGEX_STAR) {
            return matchStar(regex, (regex + 2), text, matcher);

        } else if (regex[1].patternType == REGEX_LAZY_STAR) {
            return matchStarLazy(regex, (regex + 2), text, matcher);

        } else if (regex[1].patternType == REGEX_PLUS) {
            return matchPlus(regex, (regex + 2), text, matcher);

        } else if (regex[1].patternType == REGEX_LAZY_PLUS) {
            return matchPlusLazy(regex, (regex + 2), text, matcher);

        } else if ((regex[0].patternType == REGEX_DOLLAR_END) && regex[1].patternType == REGEX_END_OF_PATTERN) {
            return (text[0] == END_LINE);
        }

        matcher->matchLength++;
    } while (text[0] != END_LINE && matchOne(regex++, *text++));

    matcher->matchLength = previousMatch;
    return false;
}


static bool matchQuestionMark(RegexNode *regex, RegexNode *pattern, const char *text, Matcher *matcher) {
    if (regex->patternType == REGEX_END_OF_PATTERN || matchPattern(pattern, matcher, text)) {
        return true;
    }

    if (*text && matchOne(regex, *text++)) {
        if (matchPattern(pattern, matcher, text)) {
            matcher->matchLength++;
            return true;
        }
    }
    return false;
}

static bool matchQuantifier(RegexNode *regex, RegexNode *pattern, const char *text, Matcher *matcher) {
    int32_t preLength = matcher->matchLength;
    uint16_t minQuantifier = pattern->minMaxQuantifiers[0];
    int32_t maxQuantifier = pattern->minMaxQuantifiers[1] - minQuantifier;

    while (text[0] != END_LINE && minQuantifier > 0 && matchOne(regex, *text)) {
        matcher->matchLength++;
        minQuantifier--;
        text++;
    }

    if (minQuantifier > 0) {
        return false;
    }

    do {
        if (matchPattern(pattern + 1, matcher, text)) {
            return true;
        }
        matcher->matchLength++;
    } while (text[0] != END_LINE && maxQuantifier-- > 0 && matchOne(regex, *text++));

    matcher->matchLength = preLength;
    return false;
}

static bool matchStar(RegexNode *regex, RegexNode *pattern, const char *text, Matcher *matcher) {
    const char *prePoint = text;
    while (text[0] != END_LINE && matchOne(regex, *text)) {
        matcher->matchLength++;
        text++;
    }

    if (matcher->matchLength == 0) {
        return false;
    }

    while (text >= prePoint) {
        if (matchPattern(pattern, matcher, text--)) {
            return true;
        }
        matcher->matchLength--;
    }
    return false;
}

static bool matchStarLazy(RegexNode *regex, RegexNode *pattern, const char *text, Matcher *matcher) {
    int32_t preLength = matcher->matchLength;

    do {
        if (matchPattern(pattern, matcher, text)) {
            return true;
        }
        matcher->matchLength++;
    } while (text[0] != END_LINE && matchOne(regex, *text++));

    matcher->matchLength = preLength;
    return false;
}

static bool matchPlus(RegexNode *regex, RegexNode *pattern, const char *text, Matcher *matcher) {
    const char *prePoint = text;
    while ((text[0] != END_LINE) && matchOne(regex, *text)) {
        matcher->matchLength++;
        text++;
    }

    while (text > prePoint) {   // match one or more
        if (matchPattern(pattern, matcher, text--)) {
            return true;
        }
        matcher->matchLength--;
    }
    matcher->matchLength = 0;
    return false;
}

static bool matchPlusLazy(RegexNode *regex, RegexNode *pattern, const char *text, Matcher *matcher) {
    while ((text[0] != END_LINE) && matchOne(regex, *text)) {
        matcher->matchLength++;
        text++;

        if (matchPattern(pattern, matcher, text)) {
            return true;
        }
    }
    matcher->matchLength--;
    return false;
}

static bool matchOne(RegexNode *regex, char character) {
    switch (regex->patternType) {
        case REGEX_DOT:
            return isMatchingDot(character);
        case REGEX_CHAR_CLASS:
            return matchCharClass(character, regex->classCharPtr);
        case REGEX_INVERSE_CHAR_CLASS:
            return !matchCharClass(character, regex->classCharPtr);
        case REGEX_DIGIT:
            return isdigit(character);
        case REGEX_NOT_DIGIT:
            return !isdigit(character);
        case REGEX_ALPHA:
            return isalnum(character);
        case REGEX_NOT_ALPHA:
            return !isalnum(character);
        case REGEX_WHITESPACE:
            return isspace(character);
        case REGEX_NOT_WHITESPACE:
            return !isspace(character);
        case REGEX_REGULAR_CHAR:
            return (regex->regexChar == character);
        default:
            return false;
    }
}

static inline bool isMatchingDot(unsigned char character) {
#if defined(REGEX_DOT_MATCH_NEWLINE) && (REGEX_DOT_MATCH_NEWLINE == true)
    (void) character;
    return true;
#else
    return (character != '\n' && character != '\r');
#endif
}

static bool matchCharClass(unsigned char character, const unsigned char *metaCharString) {
    do {
        if (isMatchingRange(character, metaCharString)) {
            return true;
        } else if (metaCharString[0] == '\\') { /* Escape-char: increment metaCharString-ptr and regexMatch on next char */
            metaCharString++;
            if (isMatchingMetaChar(character, metaCharString)) {
                return true;
            }
        } else if (character == metaCharString[0]) {
            return (character) == '-' ? (metaCharString[-1] == END_LINE) || (metaCharString[1] == END_LINE) : true;
        }
    } while (*metaCharString++ != END_LINE);

    return false;
}

static inline bool isMatchingRange(unsigned char character, const unsigned char *string) {
    return ((character != '-')
            && (string[0] != END_LINE)
            && (string[0] != '-')
            && (string[1] == '-')
            && (string[2] != END_LINE)
            && ((character >= string[0]) && (character <= string[2])));
}

static bool isMatchingMetaChar(unsigned char character, const unsigned char *metaCharString) {
    unsigned char metaChar = metaCharString[0];
    switch (metaChar) {
        case 'd':
            return isdigit(character);
        case 'D':
            return !isdigit(character);
        case 'w':
            return isalnum(character);
        case 'W':
            return !isalnum(character);
        case 's':
            return isspace(character);
        case 'S':
            return !isspace(character);
        default:
            return (character == metaChar);
    }
}

uint64_t substitute_(const char *haystack, const char *pattern,
    const char *replacement, bool greedy,
    char *buffer, uint64_t bufferLength,
    bool *successful, const char **errorMessage, ...
) {
    uint64_t bufferPosition = 0;
    uint64_t haystackPosition = 0;
    uint64_t replacementLength = 0;
    uint64_t copyLength = 0;

    if ((haystack == NULL) || (pattern == NULL)
        || (replacement == NULL) || (buffer == NULL)
    ) {
        if (successful != NULL) {
            *successful = false;
        }
        if (errorMessage != NULL) {
            *errorMessage = "One or more NULL parameters to substitute.\n";
        }
        return bufferPosition; // 0
    }
    replacementLength = (uint64_t) strlen(replacement);

    if (errorMessage != NULL) {
        // Initialize *errorMessage to NULL so that we can tell when we've
        // already set the value before.
        *errorMessage = NULL;
    }

    Regex regex;
    regexCompileLength(&regex, pattern, 0);
    if (!regex.isPatternValid) {
        if (successful != NULL) {
            *successful = false;
        }
        if (errorMessage != NULL) {
            *errorMessage = regex.errorMessage;
        }
        goto final;
    }

    Matcher matcher;
    regexMatchMatcher(&regex, &haystack[haystackPosition], &matcher);
    while (matcher.isFound) {
        copyLength = (haystackPosition + matcher.foundAtIndex) - haystackPosition;
        if ((bufferPosition + copyLength) < bufferLength) {
            memcpy(&buffer[bufferPosition], &haystack[haystackPosition], copyLength);
        } else {
            if (successful != NULL) {
                *successful = false;
            }
            if ((errorMessage != NULL) && (*errorMessage == NULL)) {
                // We want error message to hold the FIRST error, so don't set
                // it again if it's already set.
                *errorMessage
                    = "Provided output buffer too small for replaced output.";
            }
        }
        bufferPosition += copyLength;
        haystackPosition += copyLength;

        if ((bufferPosition + replacementLength) < bufferLength) {
            memcpy(&buffer[bufferPosition], replacement, replacementLength);
        } else {
            if (successful != NULL) {
                *successful = false;
            }
            if ((errorMessage != NULL) && (*errorMessage == NULL)) {
                // We want error message to hold the FIRST error, so don't set
                // it again if it's already set.
                *errorMessage
                    = "Provided output buffer too small for replaced output.";
            }
        }
        bufferPosition += replacementLength;
        haystackPosition += matcher.matchLength;

        if (greedy == true) {
            regexMatchMatcher(&regex, &haystack[haystackPosition], &matcher);
        } else {
            break;
        }
    }

    if (successful != NULL) {
        *successful = true;
    }

final:
    // Take care of the last of the string.
    copyLength = ((uint64_t) strlen(haystack)) - haystackPosition;
    if ((bufferPosition + copyLength) < bufferLength) {
        memcpy(&buffer[bufferPosition], &haystack[haystackPosition], copyLength);
    } else {
        if (successful != NULL) {
            *successful = false;
        }
        if ((errorMessage != NULL) && (*errorMessage == NULL)) {
            // We want error message to hold the FIRST error, so don't set
            // it again if it's already set.
            *errorMessage
                = "Provided output buffer too small for replaced output.";
        }
    }
    bufferPosition += copyLength;

    if (bufferPosition < bufferLength) {
        buffer[bufferPosition] = '\0';
    } else {
        buffer[bufferPosition - copyLength] = '\0';
    }

    return bufferPosition;
}

uint64_t substituteMultiple_(const char *haystack, Substitution *substitutions,
    bool greedy, char **buffers, uint64_t bufferLength, unsigned int *finalIndex,
    bool *successful, const char **errorMessage, SubstituteFunction substituteFunction, ...
) {
    uint64_t maxReplacementLength = 0;
    uint64_t replacementLength = 0;
    if ((haystack == NULL) || (substitutions == NULL)
        || (buffers == NULL) || (buffers[0] == NULL) || (buffers[1] == NULL)
        || (finalIndex == NULL)
    ) {
        if (successful != NULL) {
            *successful = false;
        }
        if (errorMessage != NULL) {
            *errorMessage = "One or more NULL parameters to substituteMultiple.\n";
        }
        return replacementLength; // 0
    }

    if (errorMessage != NULL) {
        // Clear it out.
        *errorMessage = NULL;
    }

    if (substituteFunction == NULL) {
        substituteFunction = substitute_;
    }

    const char *input = haystack;
    unsigned int bufferIndex = 0;
    char *output = NULL;

    bool allSuccessful = true; // Until proven false.
    bool substituteSuccessful = false; // Until proven true;
    const char *substituteErrorMessage = NULL;
    for (uint64_t ii = 0;
        ((substitutions[ii].pattern != NULL) && (substitutions[ii].replacement != NULL));
        ii++
    ) {
        output = buffers[bufferIndex];
        replacementLength = substituteFunction(input,
            substitutions[ii].pattern, substitutions[ii].replacement,
            greedy, output, bufferLength,
            &substituteSuccessful, &substituteErrorMessage);
        if (replacementLength > maxReplacementLength) {
            maxReplacementLength = replacementLength;
        }
        allSuccessful &= substituteSuccessful;
        // We want the error message reported by this function to be the FIRST
        // error message we encounter.  Since substitute only sets the error
        // message on failure, we can just blindly set the errorMessage pointer
        // we were provided to substituteErrorMessage until errorMessage is set.
        if ((errorMessage != NULL) && (*errorMessage == NULL)) {
            *errorMessage = substituteErrorMessage;
        }

        // Use the current output buffer as the input buffer on the next pass.
        input = (const char*) output;
        // Switch to the other buffer for the next pass.
        bufferIndex ^= 1;
    }

    bufferIndex ^= 1;
    *finalIndex = bufferIndex;

    if (successful != NULL) {
        *successful = allSuccessful;
    }
    return maxReplacementLength;
}

typedef struct Subexpression {
    char value[MAX_CHAR_CLASS_LENGTH];
    uint64_t length;
} Subexpression;
    
static inline int getSubexpressions(const char *pattern,
    Subexpression subexpressions[MAX_SUBEXPRESSIONS]
) {
    int numSubexpressions = 0;

    const char *subexpressionStart = strstr(pattern, "\\(");
    for (;
        (subexpressionStart) && (numSubexpressions < MAX_SUBEXPRESSIONS - 1);
        numSubexpressions++
    ) {
        const char *subexpressionEnd = strstr(subexpressionStart, "\\)");
        if (!subexpressionEnd) {
            // Not a properly-formed subexpression.  Bail.
            break;
        }

        uint64_t initialExpressionLength = (uint64_t) (((uintptr_t) subexpressionStart) - ((uintptr_t) pattern));
        if (initialExpressionLength > 0) {
            strncpy(subexpressions[numSubexpressions].value, pattern, initialExpressionLength);
            subexpressions[numSubexpressions].value[initialExpressionLength] = '\0';
            subexpressions[numSubexpressions].length = initialExpressionLength;
            numSubexpressions++;
            if (numSubexpressions == MAX_SUBEXPRESSIONS - 1) {
                break;
            }
        }

        uint64_t subexpressionLength = (uint64_t) (((uintptr_t) subexpressionEnd + 2) - ((uintptr_t) subexpressionStart));
        strncpy(subexpressions[numSubexpressions].value, subexpressionStart, subexpressionLength);
        subexpressions[numSubexpressions].value[subexpressionLength] = '\0';
        subexpressions[numSubexpressions].length = subexpressionLength;

        pattern = subexpressionEnd + 2;
        subexpressionStart = strstr(pattern, "\\(");
    }

    if (*pattern != '\0') {
        subexpressions[numSubexpressions].length = strlen(pattern);
        memcpy(subexpressions[numSubexpressions].value, pattern, subexpressions[numSubexpressions].length + 1);
        numSubexpressions++;
    }

    return numSubexpressions;
}

static inline int getReplacements(const char *replacementString,
    char replacements[MAX_SUBEXPRESSIONS][MAX_CHAR_CLASS_LENGTH]
) {
    int numReplacements = 0;

    const char *backslashAt = strchr(replacementString, '\\');
    while ((backslashAt) && (numReplacements < MAX_SUBEXPRESSIONS - 1)) {
        char nextChar = *(backslashAt + 1);
        if ((nextChar < '0') || (nextChar > '9')) {
            replacementString += 2;
            backslashAt = strchr(replacementString, '\\');
            continue;
        }

        uint64_t replacementLength = (uint64_t) (((uintptr_t) backslashAt) - ((uintptr_t) replacementString));
        if (replacementLength > 0) {
            strncpy(replacements[numReplacements], replacementString, replacementLength);
            replacements[numReplacements][replacementLength] = '\0';
            numReplacements++;
            replacementString = backslashAt;
        }

        backslashAt = strchr(replacementString + 1, '\\');
    }

    if (*replacementString != '\0') {
        strcpy(replacements[numReplacements], replacementString);
        numReplacements++;
    }

    return numReplacements;
}

uint64_t substituteMatch_(const char *haystack, const char *pattern,
    const char *replacement, bool greedy,
    char *buffer, uint64_t bufferLength,
    bool *successful, const char **errorMessage, ...
) {
    uint64_t bufferPosition = 0;
    uint64_t haystackPosition = 0;
    uint64_t haystackLength = 0;
    uint64_t copyLength = 0;
    uint64_t numIterations = 0;
    uint64_t lastMatchPosition = 0;
    Regex regex;
    Matcher matcher;

    if ((haystack == NULL) || (pattern == NULL)
        || (replacement == NULL) || (buffer == NULL)
    ) {
        if (successful != NULL) {
            *successful = false;
        }
        if (errorMessage != NULL) {
            *errorMessage = "One or more NULL parameters to substituteMatch.\n";
        }
        return bufferPosition; // 0
    }
    haystackLength = (uint64_t) strlen(haystack);

    Subexpression subexpressions[MAX_SUBEXPRESSIONS];
    int numSubexpressions = getSubexpressions(pattern, subexpressions);

    char replacements[MAX_SUBEXPRESSIONS][MAX_CHAR_CLASS_LENGTH];
    int numReplacements = getReplacements(replacement, replacements);

    if (errorMessage != NULL) {
        // Initialize *errorMessage to NULL so that we can tell when we've
        // already set the value before.
        *errorMessage = NULL;
    }

    while ((numIterations < 1) || ((greedy == true) && (haystackPosition < haystackLength))) {
        // matches[0] will hold the entire match, so we need one more than MAX_SUBEXPRESSIONS.
        char matches[MAX_SUBEXPRESSIONS + 1][MAX_CHAR_CLASS_LENGTH];
        uint64_t match0Length = 0;
        int numMatches = 1;

        // First, we need to find all the matches.
        uint64_t firstMatchPosition = 0;
        for (int ii = 0; ii < numSubexpressions; ii++) {
            const char *pattern = subexpressions[ii].value; // shadows input parameter
            size_t patternLength = subexpressions[ii].length;
            bool storeMatch = false;
            if ((pattern[0] == '\\') && (pattern[1] == '(')) {
              pattern += 2;
              patternLength -= 4;
              storeMatch = true;
            }

            regexCompileLength(&regex, pattern, patternLength);
            if (!regex.isPatternValid) {
                if (successful != NULL) {
                    *successful = false;
                }
                if (errorMessage != NULL) {
                    *errorMessage = regex.errorMessage;
                }
                goto final;
            }

            regexMatchMatcher(&regex, &haystack[lastMatchPosition], &matcher);
            if (matcher.isFound) {
                matcher.foundAtIndex += lastMatchPosition;
                if (ii == 0) {
                    firstMatchPosition = matcher.foundAtIndex;
                }

                if (match0Length + matcher.matchLength < MAX_CHAR_CLASS_LENGTH) {
                    memcpy(&matches[0][match0Length], &haystack[matcher.foundAtIndex], matcher.matchLength);
                    match0Length += matcher.matchLength;
                } else {
                    copyLength = MAX_CHAR_CLASS_LENGTH - match0Length - 1;
                    memcpy(&matches[0][match0Length], &haystack[matcher.foundAtIndex], copyLength);
                    match0Length = MAX_CHAR_CLASS_LENGTH - 1;
                }
                matches[0][match0Length] = '\0';

                if (storeMatch) {
                    if (matcher.matchLength < MAX_CHAR_CLASS_LENGTH) {
                        memcpy(matches[numMatches], &haystack[matcher.foundAtIndex], matcher.matchLength);
                        matches[numMatches][matcher.matchLength] = '\0';
                        numMatches++;
                    } else {
                        if (successful != NULL) {
                            *successful = false;
                        }
                        if ((errorMessage != NULL) && (*errorMessage == NULL)) {
                            // We want error message to hold the FIRST error, so don't set
                            // it again if it's already set.
                            *errorMessage
                                = "Matched expression is more than " STRINGIFY(MAX_CHAR_CLASS_LENGTH) " characters long.";
                            numMatches = 0;
                            goto final;
                        }
                    }
                }

                lastMatchPosition = matcher.foundAtIndex + matcher.matchLength;
            } else {
                // This isn't valid.  We either match all of the subexpressions or none of them.
                numMatches = 0;
                break;
            }
        }

        // Now, we need to replace the parts of the input string with the pieces
        // specified in the replacement.  Matches from the original input string are
        // identified by the syntax "\index" where index is a non-negative integer.
        // "\0" will yield the entier match (or as much of it as we could store).
        // If an integer is not present then the replacement is taken as-is.
        if (numMatches > 0) {
            // First, we need to grab everything before the match and start our output
            // with that.
            copyLength = firstMatchPosition - haystackPosition;
            if ((bufferPosition + copyLength) < bufferLength) {
                memcpy(&buffer[bufferPosition], &haystack[haystackPosition], copyLength);
                bufferPosition += copyLength;
                haystackPosition += copyLength;
            } else {
                if (successful != NULL) {
                    *successful = false;
                }
                if ((errorMessage != NULL) && (*errorMessage == NULL)) {
                    // We want error message to hold the FIRST error, so don't set
                    // it again if it's already set.
                    *errorMessage
                        = "Provided output buffer too small for replaced output.";
                    goto final;
                }
            }

            // Next, append the replacements to the output buffer.
            for (int ii = 0; ii < numReplacements; ii++) {
                long matchIndex = 0;
                char *matchIndexStart = replacements[ii] + 1;
                char *endPtr = matchIndexStart;
                if (replacements[ii][0] == '\\') {
                    matchIndex = strtol(matchIndexStart, &endPtr, 10);
                }
                if (endPtr != matchIndexStart) {
                    // matchIndex was parsed into a valid number.  Use it.
                    if (matchIndex < numMatches) {
                        copyLength = strlen(matches[matchIndex]);
                        if ((bufferPosition + copyLength) < bufferLength) {
                            memcpy(&buffer[bufferPosition], matches[matchIndex], copyLength);
                            bufferPosition += copyLength;
                        } else {
                            if (successful != NULL) {
                                *successful = false;
                            }
                            if ((errorMessage != NULL) && (*errorMessage == NULL)) {
                                // We want error message to hold the FIRST error, so don't set
                                // it again if it's already set.
                                *errorMessage
                                    = "Provided output buffer too small for replaced output.";
                                goto final;
                            }
                        }

                        // Anything after the integer should be appended literally.
                        copyLength = strlen(endPtr);
                        if ((bufferPosition + copyLength) < bufferLength) {
                            memcpy(&buffer[bufferPosition], endPtr, copyLength);
                            bufferPosition += copyLength;
                        } else {
                            if (successful != NULL) {
                                *successful = false;
                            }
                            if ((errorMessage != NULL) && (*errorMessage == NULL)) {
                                // We want error message to hold the FIRST error, so don't set
                                // it again if it's already set.
                                *errorMessage
                                    = "Provided output buffer too small for replaced output.";
                                goto final;
                            }
                        }
                    } else {
                          if (successful != NULL) {
                              *successful = false;
                          }
                          if ((errorMessage != NULL) && (*errorMessage == NULL)) {
                              // We want error message to hold the FIRST error, so don't set
                              // it again if it's already set.
                              *errorMessage = "Invalid match index referenced.";
                              goto final;
                          }
                    }
                } else {
                    // replacements[ii] is just a string.  Use it directly.
                    copyLength = strlen(replacements[ii]);
                    if ((bufferPosition + copyLength) < bufferLength) {
                        memcpy(&buffer[bufferPosition], replacements[ii], copyLength);
                        bufferPosition += copyLength;
                    } else {
                        if (successful != NULL) {
                            *successful = false;
                        }
                        if ((errorMessage != NULL) && (*errorMessage == NULL)) {
                            // We want error message to hold the FIRST error, so don't set
                            // it again if it's already set.
                            *errorMessage
                                = "Provided output buffer too small for replaced output.";
                            goto final;
                        }
                    }
                }
            }
        } else {
            // Error case.  Bail.
            break;
        }

        haystackPosition = lastMatchPosition;
        numIterations++;
    }

    if (successful != NULL) {
        *successful = true;
    }

final:
    // Take care of the last of the string.
    copyLength = haystackLength - haystackPosition;
    if ((bufferPosition + copyLength) < bufferLength) {
        memcpy(&buffer[bufferPosition], &haystack[haystackPosition], copyLength);
    } else {
        if (successful != NULL) {
            *successful = false;
        }
        if ((errorMessage != NULL) && (*errorMessage == NULL)) {
            // We want error message to hold the FIRST error, so don't set
            // it again if it's already set.
            *errorMessage
                = "Provided output buffer too small for replaced output.";
        }
    }
    bufferPosition += copyLength;

    if (bufferPosition < bufferLength) {
        buffer[bufferPosition] = '\0';
    } else {
        buffer[bufferPosition - copyLength] = '\0';
    }

    return bufferPosition;
}

