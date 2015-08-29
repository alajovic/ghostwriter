/***********************************************************************
 *
 * Copyright (C) 2014, 2015 wereturtle
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ***********************************************************************/

#include <QString>
#include <QChar>
#include <QRegExp>

#include "MarkdownParser.h"
#include "MarkdownStates.h"

// This character is used to replace escape characters and other characters
// with special meaning in a dummy copy of the current line being parsed,
// for ease of parsing.
//
static const QChar DUMMY_CHAR('@');

static const int MAX_MARKDOWN_HEADING_LEVEL = 6;


MarkdownParser::MarkdownParser()
{
    paragraphBreakRegex.setPattern("^\\s*$");
    heading1SetextRegex.setPattern("^===+\\s*$");
    heading2SetextRegex.setPattern("^---+\\s*$");
    blockquoteRegex.setPattern("^ {0,3}>.*$");
    githubCodeFenceStartRegex.setPattern("^```+.*$");
    githubCodeFenceEndRegex.setPattern("^```+\\s*$");
    pandocCodeFenceStartRegex.setPattern("^~~~+.*$");
    pandocCodeFenceEndRegex.setPattern("^~~~+\\s*$");
    numberedListRegex.setPattern("^ {0,3}[0-9]+[.)]\\s+.*$");
    numberedNestedListRegex.setPattern("^\\s*[0-9]+[.)]\\s+.*$");
    hruleRegex.setPattern("\\s*(\\*\\s*){3,}|(\\s*(_\\s*){3,})|((\\s*(-\\s*){3,}))");
    emphasisRegex.setPattern("(\\*(?![\\s*]).*[^\\s*]\\*)|_(?![\\s_]).*[^\\s_]_");
    emphasisRegex.setMinimal(true);
    strongRegex.setPattern("\\*\\*(?=\\S).*\\S\\*\\*(?!\\*)|__(?=\\S).*\\S__(?!_)");
    strongRegex.setMinimal(true);
    strikethroughRegex.setPattern("~~.*~~");
    strikethroughRegex.setMinimal(true);
    verbatimRegex.setPattern("`[^`]+`|``+.+``+");
    verbatimRegex.setMinimal(true);
    htmlTagRegex.setPattern("<[^<>]+>");
    htmlTagRegex.setMinimal(true);
    htmlEntityRegex.setPattern("&[a-zA-Z]+;|&#x?[0-9]+;");
    automaticLinkRegex.setPattern("(<[a-zA-Z]+\\:.+>)|(<.+@.+>)");
    automaticLinkRegex.setMinimal(true);
    inlineLinkRegex.setPattern("\\[.+\\]\\(.+\\)");
    inlineLinkRegex.setMinimal(true);
    referenceLinkRegex.setPattern("\\[(.+)\\]");
    referenceLinkRegex.setMinimal(true);
    referenceDefinitionRegex.setPattern("^\\s*\\[.+\\]:");
    imageRegex.setPattern("!\\[.*\\]\\(.+\\)");
    imageRegex.setMinimal(true);
    htmlInlineCommentRegex.setPattern("<!--.*-->");
    htmlInlineCommentRegex.setMinimal(true);
}
        
MarkdownParser::~MarkdownParser()
{
    ;
}

void MarkdownParser::parseLine
(
    const QString& text,
    int currentState,
    int previousState,
    const int nextState
)
{
    this->currentState = currentState;
    this->previousState = previousState;
    this->nextState = nextState;

    if
    (
        (MarkdownStateCodeBlock != previousState)
        && (MarkdownStateInGithubCodeFence != previousState)
        && (MarkdownStateInPandocCodeFence != previousState)
        && (MarkdownStateComment != previousState)
        && paragraphBreakRegex.exactMatch(text)
    )
    {
        if
        (
            (MarkdownStateListLineBreak == previousState)
            || (MarkdownStateNumberedList == previousState)
            || (MarkdownStateBulletPointList == previousState)
        )
        {
            setState(MarkdownStateListLineBreak);
        }
        else if
        (
            (MarkdownStateCodeBlock != previousState)
            ||
            (
                !text.startsWith("\t")
                && !text.endsWith("    ")
            )
        )
        {
            setState(MarkdownStateParagraphBreak);
        }
    }
    else if
    (
        parseSetextHeadingLine2(text)
        || parseCodeBlock(text)
        || parseMultilineComment(text)
        || parseHorizontalRule(text)
    )
    {
        ; // No further tokenizing required
    }
    else if 
    (
        parseAtxHeading(text)
        || parseSetextHeadingLine1(text)
        || parseBlockquote(text)
        || parseNumberedList(text)
        || parseBulletPointList(text)
    )
    {
        parseInline(text);
    }
    else
    {
        if
        (
            (MarkdownStateListLineBreak == previousState)
            || (MarkdownStateNumberedList == previousState)
            || (MarkdownStateBulletPointList == previousState)
        )
        {
            if
            (
                !parseNumberedList(text)
                && !parseBulletPointList(text)
                && (text.startsWith("\t") || text.startsWith("    "))
            )
            {
                setState(previousState);
            }
            else
            {
                setState(MarkdownStateParagraph);
            }
        }
        else
        {
            setState(MarkdownStateParagraph);
        }

        // tokenize inline
        parseInline(text);
    }

    // Make sure that if the second line of a setext heading is removed the
    // first line is reprocessed.  Otherwise, it will still show up in the
    // document as a heading.
    //
    if
    (
        (
            (previousState == MarkdownStateSetextHeading1Line1)
            && (this->getState() != MarkdownStateSetextHeading1Line2)
        )
        ||
        (
            (previousState == MarkdownStateSetextHeading2Line1)
            && (this->getState() != MarkdownStateSetextHeading2Line2)
        )
    )
    {
        this->requestBacktrack();
    }
}

bool MarkdownParser::parseSetextHeadingLine1
(
    const QString& text
)
{
    // Check the next line's state to see if this is a setext-style heading.
    //
    int level = 0;
    Token token;

    if (MarkdownStateSetextHeading1Line2 == nextState)
    {
        level = 1;
        this->setState(MarkdownStateSetextHeading1Line1);
        token.setType(TokenSetextHeading1Line1);
    }
    else if (MarkdownStateSetextHeading2Line2 == nextState)
    {
        level = 2;
        this->setState(MarkdownStateSetextHeading2Line1);
        token.setType(TokenSetextHeading2Line1);
    }

    if (level > 0)
    {
        token.setLength(text.length());
        token.setPosition(0);
        this->addToken(token);
        return true;
    }

    return false;
}

bool MarkdownParser::parseSetextHeadingLine2
(
    const QString& text
)
{
    int level = 0;
    bool setextMatch = false;
    Token token;

    if (MarkdownStateSetextHeading1Line1 == previousState)
    {
        level = 1;
        setextMatch = heading1SetextRegex.exactMatch(text);
        setState(MarkdownStateSetextHeading1Line2);
        token.setType(TokenSetextHeading1Line2);
    }
    else if (MarkdownStateSetextHeading2Line1 == previousState)
    {
        level = 2;
        setextMatch = heading2SetextRegex.exactMatch(text);
        setState(MarkdownStateSetextHeading2Line2);
        token.setType(TokenSetextHeading2Line2);
    }
    else if (MarkdownStateParagraph == previousState)
    {
        bool h1Line2 = heading1SetextRegex.exactMatch(text);
        bool h2Line2 = heading2SetextRegex.exactMatch(text);

        if (h1Line2 || h2Line2)
        {
            // Restart tokenizing on the previous line.
            this->requestBacktrack();
            token.setLength(text.length());
            token.setPosition(0);

            if (h1Line2)
            {
                setState(MarkdownStateSetextHeading1Line2);
                token.setType(TokenSetextHeading1Line2);
            }
            else
            {
                setState(MarkdownStateSetextHeading2Line2);
                token.setType(TokenSetextHeading2Line2);
            }

            this->addToken(token);
            return true;
        }
    }

    if (level > 0)
    {
        if (setextMatch)
        {
            token.setLength(text.length());
            token.setPosition(0);
            this->addToken(token);
            return true;
        }
        else
        {
            // Restart tokenizing on the previous line.
            this->requestBacktrack();
            return false;
        }
    }

    return false;
}

bool MarkdownParser::parseAtxHeading(const QString& text)
{
    QString escapedText = dummyOutEscapeCharacters(text);
    int trailingPoundCount = 0;

    int level = 0;

    // Count the number of pound signs at the front of the string,
    // up to the maximum allowed, to determine the heading level.
    //
    for
    (
        int i = 0;
        ((i < escapedText.length()) && (i < MAX_MARKDOWN_HEADING_LEVEL));
        i++
    )
    {
        if (QChar('#') == escapedText[i])
        {
            level++;
        }
        else
        {
            // We're done counting, as no more pound signs are available.
            break;
        }
    }

    if ((level > 0) && (level < text.length()))
    {
        // Count how many pound signs are at the end of the text.
        for (int i = escapedText.length() - 1; i > level; i--)
        {
            if (QChar('#') == escapedText[i])
            {
                trailingPoundCount++;
            }
            else
            {
                // We're done counting, as no more pound signs are available.
                break;
            }
        }

        Token token;
        token.setPosition(0);
        token.setLength(text.length());
        token.setType((MarkdownTokenType) (TokenAtxHeading1 + level - 1));
        token.setOpeningMarkupLength(level);
        token.setClosingMarkupLength(trailingPoundCount);
        this->addToken(token);
        setState(MarkdownStateAtxHeading1 + level - 1);
        return true;
    }

    return false;
}

bool MarkdownParser::parseNumberedList
(
    const QString& text
)
{
    if
    (
        (
            (
                (MarkdownStateParagraphBreak == previousState)
                || (MarkdownStateUnknown == previousState)
            )
            && numberedListRegex.exactMatch(text)
        )
        ||
        (
            (
                (MarkdownStateListLineBreak == previousState)
                || (MarkdownStateNumberedList == previousState)
            )
            && numberedNestedListRegex.exactMatch(text)
        )
    )
    {
        int periodIndex = text.indexOf(QChar('.'));
        int parenthIndex = text.indexOf(QChar(')'));
        int index = -1;

        if (periodIndex < 0)
        {
            index = parenthIndex;
        }
        else if (parenthIndex < 0)
        {
            index = periodIndex;
        }
        else if (parenthIndex > periodIndex)
        {
            index = periodIndex;
        }
        else
        {
            index = parenthIndex;
        }
        
        if (index >= 0)
        {
            Token token;
            token.setType(TokenNumberedList);
            token.setPosition(0);
            token.setLength(text.length());
            token.setOpeningMarkupLength(index + 1);
            this->addToken(token);
            setState(MarkdownStateNumberedList);
            return true;
        }
        
        return false;
    }

    return false;
}

bool MarkdownParser::parseBulletPointList
(
    const QString& text
)
{
    bool foundBulletChar = false;
    int bulletCharIndex = -1;
    int spaceCount = 0;
    bool whitespaceFoundAfterBulletChar = false;

    if 
    (
        (MarkdownStateUnknown != previousState)
        && (MarkdownStateParagraphBreak != previousState)
        && (MarkdownStateListLineBreak != previousState)
        && (MarkdownStateNumberedList != previousState)
        && (MarkdownStateBulletPointList != previousState)
    )
    {
        return false;
    }

    // Search for the bullet point character, which can
    // be either a '+', '-', or '*'.
    //
    for (int i = 0; i < text.length(); i++)
    {
        if (QChar(' ') == text[i])
        {
            if (foundBulletChar)
            {
                // We've confirmed it's a bullet point by the whitespace that
                // follows the bullet point character, and can now exit the
                // loop.
                //
                whitespaceFoundAfterBulletChar = true;
                break;
            }
            else
            {
                spaceCount++;

                // If this list item is the first in the list, ensure the
                // number of spaces preceeding the bullet point does not
                // exceed three, as that would indicate a code block rather
                // than a bullet point list.
                //
                if
                (
                    (spaceCount > 3)
                    && (MarkdownStateNumberedList != previousState)
                    && (MarkdownStateBulletPointList != previousState)
                    && (MarkdownStateListLineBreak != previousState)
                    &&
                    (
                        (MarkdownStateParagraphBreak == previousState)
                        || (MarkdownStateUnknown == previousState)
                    )
                )
                {
                    return false;
                }
            }
        }
        else if (QChar('\t') == text[i])
        {
            if (foundBulletChar)
            {
                // We've confirmed it's a bullet point by the whitespace that
                // follows the bullet point character, and can now exit the
                // loop.
                //
                whitespaceFoundAfterBulletChar = true;
                break;
            }
            else if
            (
                (MarkdownStateParagraphBreak == previousState)
                || (MarkdownStateUnknown == previousState)
            )
            {
                // If this list item is the first in the list, ensure that
                // no tab character preceedes the bullet point, as that would
                // indicate a code block rather than a bullet point list.
                //
                return false;
            }
        }
        else if 
        (
            (QChar('+') == text[i])
            || (QChar('-') == text[i])
            || (QChar('*') == text[i])
        )
        {
            foundBulletChar = true;
            bulletCharIndex = i;
        }
        else
        {
            return false;
        }
    }

    if ((bulletCharIndex >= 0) && whitespaceFoundAfterBulletChar)
    {
        Token token;
        token.setType(TokenBulletPointList);
        token.setPosition(0);
        token.setLength(text.length());
        token.setOpeningMarkupLength(bulletCharIndex + 1);
        this->addToken(token);
        setState(MarkdownStateBulletPointList);
        return true;
    }

    return false;
}

bool MarkdownParser::parseHorizontalRule(const QString& text)
{
    if (hruleRegex.exactMatch(text))
    {
        Token token;
        token.setType(TokenHorizontalRule);
        token.setPosition(0);
        token.setLength(text.length());
        this->addToken(token);
        setState(MarkdownStateHorizontalRule);
        return true;
    }

    return false;
}

bool MarkdownParser::parseBlockquote
(
    const QString& text
)
{
    if 
    (
        (MarkdownStateBlockquote == previousState)
        || blockquoteRegex.exactMatch(text)
    )
    {
        // Find any '>' characters at the front of the line.
        int markupLength = 0;

        for (int i = 0; i < text.length(); i++)
        {
            if (QChar('>') == text[i])
            {
                markupLength = i + 1;
            }
            else if (!text[i].isSpace())
            {
                // There are no more '>' characters at the front of the line,
                // so stop processing.
                //
                break;
            }
        }

        Token token;
        token.setType(TokenBlockquote);
        token.setPosition(0);
        token.setLength(text.length());

        if (markupLength > 0)
        {
            token.setOpeningMarkupLength(markupLength);
        }

        addToken(token);
        setState(MarkdownStateBlockquote);
        return true;
    }
    
    return false;
}

bool MarkdownParser::parseCodeBlock
(
    const QString& text
)
{
    if 
    (
        (MarkdownStateInGithubCodeFence == previousState)
        || (MarkdownStateInPandocCodeFence == previousState)
    )
    {
        setState(previousState);

        if 
        (
            (
                (MarkdownStateInGithubCodeFence == previousState)
                && githubCodeFenceEndRegex.exactMatch(text)
            )
            ||
            (
                (MarkdownStateInPandocCodeFence == previousState)
                && pandocCodeFenceEndRegex.exactMatch(text)
            )
        )
        {
            Token token;
            token.setType(TokenCodeFenceEnd);
            token.setPosition(0);
            token.setLength(text.length());
            this->addToken(token);
            setState(MarkdownStateCodeFenceEnd);
        }
        else
        {
            Token token;
            token.setType(TokenCodeBlock);
            token.setPosition(0);
            token.setLength(text.length());
            this->addToken(token);
        }

        return true;
    }
    else if 
    (
        (
            (MarkdownStateCodeBlock == previousState)
            || (MarkdownStateParagraphBreak == previousState)
            || (MarkdownStateUnknown == previousState)
        )
        && (text.startsWith("\t") || text.startsWith("    "))
    )
    {
        Token token;
        token.setType(TokenCodeBlock);
        token.setPosition(0);
        token.setLength(text.length());
        addToken(token);
        setState(MarkdownStateCodeBlock);
        return true;
    }
    else if
    (
        (MarkdownStateParagraphBreak == previousState)
        || (MarkdownStateParagraph == previousState)
        || (MarkdownStateUnknown == previousState)           
    )
    {
        bool foundCodeFenceStart = false;
        Token token;

        if (githubCodeFenceStartRegex.exactMatch(text))
        {
            foundCodeFenceStart = true;
            token.setType(TokenGithubCodeFence);
            setState(MarkdownStateInGithubCodeFence);
        }
        else if (pandocCodeFenceStartRegex.exactMatch(text))
        {
            foundCodeFenceStart = true;
            token.setType(TokenPandocCodeFence);
            setState(MarkdownStateInPandocCodeFence);
        }

        if (foundCodeFenceStart)
        {
            token.setPosition(0);
            token.setLength(text.length());
            addToken(token);
            return true;
        }
    }
    
    return false;
}

bool MarkdownParser::parseMultilineComment
(
    const QString& text
)
{
    if (MarkdownStateComment == previousState)
    {
        // Find the end of the comment, if any.
        int index = text.indexOf("-->");
        Token token;
        token.setType(TokenHtmlComment);
        token.setPosition(0);

        if (index >= 0)
        {
            token.setLength(index + 3);
            addToken(token);

            // Return false so that the rest of the line that isn't within
            // the commented segment can be highlighted as normal paragraph
            // text.
            //
            return false;
        }
        else
        {
            token.setLength(text.length());
            addToken(token);
            setState(MarkdownStateComment);
            return true;
        }
    }

    return false;
}

bool MarkdownParser::parseInline
(
    const QString& text
)
{
    QString escapedText = dummyOutEscapeCharacters(text);

    // Check if the line is a reference definition.
    if (referenceDefinitionRegex.exactMatch(escapedText))
    {
        int colonIndex = escapedText.indexOf(':');
        Token token;
        token.setType(TokenReferenceDefinition);
        token.setPosition(0);
        token.setLength(colonIndex + 1);
        addToken(token);

        // Replace the first bracket so that the '[...]:' reference definition
        // start doesn't get highlighted as a reference link.
        //
        int firstBracketIndex = escapedText.indexOf(QChar('['));

        if (firstBracketIndex >= 0)
        {
            escapedText[firstBracketIndex] = DUMMY_CHAR;
        }
    }

    parseHtmlComments(escapedText);
    parseMatches(TokenImage, escapedText, imageRegex, 0, 0, false, true);
    parseMatches(TokenInlineLink, escapedText, inlineLinkRegex, 0, 0, false, true);
    parseMatches(TokenReferenceLink, escapedText, referenceLinkRegex);
    parseMatches(TokenHtmlEntity, escapedText, htmlEntityRegex);
    parseMatches(TokenAutomaticLink, escapedText, automaticLinkRegex, 0, 0, false, true);
    parseMatches(TokenVerbatim, escapedText, verbatimRegex, 0, 0, false, true);
    parseMatches(TokenStrikethrough, escapedText, strikethroughRegex, 2, 2);
    parseMatches(TokenStrong, escapedText, strongRegex, 2, 2, true);
    parseMatches(TokenEmphasis, escapedText, emphasisRegex, 1, 1, true);
    parseMatches(TokenHtmlTag, escapedText, htmlTagRegex);

    return true;
}

void MarkdownParser::parseHtmlComments(QString& text)
{
    // Check for the end of a multiline comment so that it doesn't get further
    // tokenized. Don't bother formatting the comment itself, however, because
    // it should have already been tokenized in tokenizeMultilineComment().
    //
    if (MarkdownStateComment == previousState)
    {
        int commentEnd = text.indexOf("-->");

        for (int i = 0; i < commentEnd + 3; i++)
        {
            text[i] = DUMMY_CHAR;
        }
    }

    // Now check for inline comments (non-multiline).
    int commentStart = text.indexOf(htmlInlineCommentRegex);

    while (commentStart >= 0)
    {
        int commentLength = htmlInlineCommentRegex.matchedLength();
        Token token;

        token.setType(TokenHtmlComment);
        token.setPosition(commentStart);
        token.setLength(commentLength);
        addToken(token);

        // Replace comment segment with dummy characters so that it doesn't
        // get tokenized again.
        //
        for (int i = commentStart; i < (commentStart + commentLength); i++)
        {
            text[i] = DUMMY_CHAR;
        }

        commentStart = text.indexOf
            (
                htmlInlineCommentRegex,
                commentStart + commentLength
            );
    }

    // Find multiline comment start, if any.
    commentStart = text.indexOf("<!--");

    if (commentStart >= 0)
    {
        Token token;

        token.setType(TokenHtmlComment);
        token.setPosition(commentStart);
        token.setLength(text.length() - commentStart);
        addToken(token);
        setState(MarkdownStateComment);

        // Replace comment segment with dummy characters so that it doesn't
        // get tokenized again.
        //
        for (int i = commentStart; i < text.length(); i++)
        {
            text[i] = DUMMY_CHAR;
        }
    }
}

void MarkdownParser::parseMatches
(
    MarkdownTokenType tokenType,
    QString& text,
    QRegExp& regex,
    const int markupStartCount,
    const int markupEndCount,
    const bool replaceMarkupChars,
    const bool replaceAllChars
)
{
    int index = text.indexOf(regex);

    while (index >= 0)
    {
        int length = regex.matchedLength();
        Token token;

        token.setType(tokenType);
        token.setPosition(index);
        token.setLength(length);

        if (markupStartCount > 0)
        {
            token.setOpeningMarkupLength(markupStartCount);
        }

        if (markupEndCount > 0)
        {
            token.setClosingMarkupLength(markupEndCount);
        }

        if (replaceAllChars)
        {
            for (int i = index; i < (index + length); i++)
            {
                text[i] = DUMMY_CHAR;
            }
        }
        else if (replaceMarkupChars)
        {
            for (int i = index; i < (index + markupStartCount); i++)
            {
                text[i] = DUMMY_CHAR;
            }

            for (int i = (index + length - markupEndCount); i < (index + length); i++)
            {
                text[i] = DUMMY_CHAR;
            }
        }

        addToken(token);
        index = text.indexOf(regex, index + length);
    }
}

QString MarkdownParser::dummyOutEscapeCharacters(const QString& text) const
{
    bool escape = false;
    QString escapedText = text;

    for (int i = 0; i < text.length(); i++)
    {
        if (escape)
        {
            escapedText[i] = DUMMY_CHAR; // Use a dummy character.
            escape = false;
        }
        else if (QChar('\\') == text[i])
        {
            escape = true;
        }
    }

    return escapedText;
}