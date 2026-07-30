#include "WikipediaActivity.h"

#include <WiFi.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <HTTPClient.h>
#include <I18n.h>
#include <Logging.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cctype>

#include "CrossPointSettings.h"
#include "SdCardFontGlobals.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "activities/util/ListLayout.h"
#include "activities/util/ListRenderHelper.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/icons/wikipediaicon.h"
#include "util/HeaderDateUtils.h"
#include "util/HtmlToTxt.h"
#include "util/NetworkMemory.h"

namespace {

// Variabile per tenere traccia del percorso del file sulla SD se l'articolo è troppo grande per la RAM
std::string g_articleFilePath;

int fontSizeToPixels(uint8_t fs) {
  switch (fs) {
    case CrossPointSettings::X_SMALL: return 14;
    case CrossPointSettings::SMALL: return 16;
    case CrossPointSettings::MEDIUM: return 18;
    case CrossPointSettings::LARGE: return 20;
    case CrossPointSettings::EXTRA_LARGE: return 22;
    default: return 16;
  }
}

int lineSpacingToLineHeight(uint8_t ls) {
  switch (ls) { case 0: return 16; case 1: return 20; case 2: return 24; case 3: return 28; default: return 20; }
}

int builtinFontId(uint8_t family, int px) {
  if (px <= 10) px = 10;
  else if (px <= 12) px = 12;
  else if (px <= 14) px = 14;
  else if (px <= 16) px = 16;
  else px = 18;

  switch (family) {
    case CrossPointSettings::BOOKERLY:
      switch (px) { case 10: return BOOKERLY_10_FONT_ID; case 12: return BOOKERLY_12_FONT_ID; case 14: return BOOKERLY_14_FONT_ID; case 16: return BOOKERLY_16_FONT_ID; default: return BOOKERLY_18_FONT_ID; }
    case CrossPointSettings::NOTOSANS:
      switch (px) { case 10: return NOTOSANS_10_FONT_ID; case 12: return NOTOSANS_12_FONT_ID; case 14: return NOTOSANS_14_FONT_ID; case 16: return NOTOSANS_16_FONT_ID; default: return NOTOSANS_18_FONT_ID; }
#ifndef OMIT_LEXEND
    case CrossPointSettings::LEXEND:
      switch (px) { case 10: return LEXEND_10_FONT_ID; case 12: return LEXEND_12_FONT_ID; case 14: return LEXEND_14_FONT_ID; case 16: return LEXEND_16_FONT_ID; default: return LEXEND_18_FONT_ID; }
#endif
    default: return BOOKERLY_18_FONT_ID;
  }
}

// ======================================================================
// URL Encoding
// ======================================================================

std::string urlEncode(const std::string& s) {
  static const char hex[] = "0123456789ABCDEF";
  std::string result;
  result.reserve(s.size() * 3);
  for (unsigned char c : s) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      result += (char)c;
    } else if (c == ' ') {
      result += "%20";
    } else {
      result += '%';
      result += hex[c >> 4];
      result += hex[c & 0x0F];
    }
  }
  return result;
}

std::string urlEncodeForPath(const std::string& s) {
  static const char hex[] = "0123456789ABCDEF";
  std::string result;
  result.reserve(s.size() * 3);
  for (unsigned char c : s) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
      result += (char)c;
    } else if (c == ' ') {
      result += '_';
    } else {
      result += '%';
      result += hex[c >> 4];
      result += hex[c & 0x0F];
    }
  }
  return result;
}

// ======================================================================
// Case-insensitive prefix check
// ======================================================================

bool startsWithCI(const char* str, size_t strLen, const char* prefix) {
  size_t plen = strlen(prefix);
  if (strLen < plen) return false;
  for (size_t i = 0; i < plen; i++) {
    if (tolower((unsigned char)str[i]) != tolower((unsigned char)prefix[i])) return false;
  }
  return true;
}

// ======================================================================
// Search Results Parser (opensearch)
// ======================================================================

int parseOpensearchTitles(const char* json, std::string* out, int max) {
  int count = 0, depth = 0; bool in = false; const char* arr = nullptr;
  const char* p = json;
  while (*p) {
    if (*p == '\\' && in) { p += 2; continue; }
    if (*p == '"') { in = !in; p++; continue; }
    if (!in && *p == '[') { depth++; if (depth == 2) { arr = p + 1; break; } }
    p++;
  }
  if (!arr) return 0;
  p = arr;
  while (*p && count < max) {
    while (*p && (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (*p == ']') break;
    if (*p == '"') {
      const char* ts = p + 1, *te = ts;
      while (*te) { if (*te == '\\' && *(te+1)) { te += 2; continue; } if (*te == '"') break; te++; }
      if (*te != '"') break;
      size_t len = te - ts;
      if (len > 0) {
        char buf[256]; size_t o = 0;
        for (const char* s = ts; s < te && o < 255; s++) {
          if (*s == '\\' && s+1 < te) { s++; switch (*s) { case 'n': buf[o++]='\n'; break; case 't': buf[o++]='\t'; break; case '\\': buf[o++]='\\'; break; case '"': buf[o++]='"'; break; default: buf[o++]=*s; break; } }
          else buf[o++] = *s;
        }
        buf[o] = '\0'; out[count] = std::string(buf, o); count++;
      }
      p = te + 1;
    } else break;
  }
  return count;
}

// ======================================================================
// Summary Extraction (REST API)
// ======================================================================

size_t extractExtractField(const char* json, char* buf, size_t sz) {
  const char* p = json; size_t o = 0;
  while (*p && o < sz - 1) {
    if (strncmp(p, "\"extract\":\"", 11) == 0) {
      p += 11;
      while (*p && o < sz - 1) {
        if (*p == '\\') {
          p++;
          switch (*p) {
            case 'n': buf[o++]='\n'; p++; break;
            case 't': buf[o++]='\t'; p++; break;
            case 'r': buf[o++]='\r'; p++; break;
            case '"': buf[o++]='"'; p++; break;
            case '\\': buf[o++]='\\'; p++; break;
            case '/': buf[o++]='/'; p++; break;
            case 'u': {
              p++;
              int val = 0;
              for (int i = 0; i < 4 && *p; i++) {
                char h = *p++;
                if (h <= '9') val = val * 16 + (h - '0');
                else val = val * 16 + ((h & 0xDF) - 'A' + 10);
              }
              if (val < 0x80) { if (o < sz-1) buf[o++] = (char)val; }
              else if (val < 0x800) { if (o < sz-2) { buf[o++] = (char)(0xC0 | (val >> 6)); buf[o++] = (char)(0x80 | (val & 0x3F)); } }
              else { if (o < sz-3) { buf[o++] = (char)(0xE0 | (val >> 12)); buf[o++] = (char)(0x80 | ((val >> 6) & 0x3F)); buf[o++] = (char)(0x80 | (val & 0x3F)); } }
              break;
            }
            default: buf[o++]=*p; p++; break;
          }
          continue;
        }
        if (*p == '"') { buf[o] = '\0'; return o; }
        buf[o++] = *p; p++;
      }
      break;
    }
    p++;
  }
  buf[o] = '\0'; return o;
}

// ======================================================================
// Wikitext Extraction from parse API JSON
// ======================================================================

std::string extractWikitextFromJson(const char* json) {
  const char* p = strstr(json, "\"wikitext\"");
  if (!p) return "";
  p = strstr(p, "\"*\"");
  if (!p) return "";
  p = strchr(p + 3, ':');
  if (!p) return "";
  p++;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  if (*p != '"') return "";
  p++;

  std::string result;
  result.reserve(4096);

  while (*p && *p != '"') {
    if (*p == '\\') {
      p++;
      switch (*p) {
        case 'n': result += '\n'; p++; break;
        case 't': result += '\t'; p++; break;
        case 'r': result += '\r'; p++; break;
        case '"': result += '"'; p++; break;
        case '\\': result += '\\'; p++; break;
        case '/': result += '/'; p++; break;
        case 'u': {
          p++;
          int val = 0;
          for (int i = 0; i < 4 && *p; i++) {
            char h = *p++;
            if (h <= '9') val = val * 16 + (h - '0');
            else val = val * 16 + ((h & 0xDF) - 'A' + 10);
          }
          if (val < 0x80) result += (char)val;
          else if (val < 0x800) {
            result += (char)(0xC0 | (val >> 6));
            result += (char)(0x80 | (val & 0x3F));
          } else {
            result += (char)(0xE0 | (val >> 12));
            result += (char)(0x80 | ((val >> 6) & 0x3F));
            result += (char)(0x80 | (val & 0x3F));
          }
          break;
        }
        default: result += *p; p++; break;
      }
    } else {
      result += *p++;
    }
  }
  return result;
}

// ======================================================================
// Inline Wikitext Formatting Processor
// ======================================================================

void processInline(const char* in, size_t inLen, char* out, size_t& o, size_t outMax) {
  const char* p = in;
  const char* end = in + inLen;

  while (p < end && o < outMax - 1) {
    // Bold/italic markers
    if (p + 2 < end && p[0] == '\'' && p[1] == '\'' && p[2] == '\'') {
      if (p + 4 < end && p[3] == '\'' && p[4] == '\'') p += 5;
      else p += 3;
      continue;
    }
    if (p + 1 < end && p[0] == '\'' && p[1] == '\'') {
      p += 2;
      continue;
    }

    // Wiki link [[...]]
    if (p + 1 < end && p[0] == '[' && p[1] == '[') {
      const char* linkStart = p + 2;
      const char* linkEnd = linkStart;
      while (linkEnd + 1 < end && !(linkEnd[0] == ']' && linkEnd[1] == ']')) linkEnd++;
      if (linkEnd + 1 < end) {
        size_t linkLen = linkEnd - linkStart;
        bool skip = false;
        if (startsWithCI(linkStart, linkLen, "File:") ||
            startsWithCI(linkStart, linkLen, "Image:") ||
            startsWithCI(linkStart, linkLen, "Aiuto:") ||
            startsWithCI(linkStart, linkLen, "Portale:") ||
            startsWithCI(linkStart, linkLen, "Template:") ||
            startsWithCI(linkStart, linkLen, "Categoria:") ||
            startsWithCI(linkStart, linkLen, "Category:") ||
            startsWithCI(linkStart, linkLen, "Wikipedia:") ||
            startsWithCI(linkStart, linkLen, "Progetto:") ||
            startsWithCI(linkStart, linkLen, "Speciale:") ||
            startsWithCI(linkStart, linkLen, "MediaWiki:")) {
          skip = true;
        }

        if (!skip) {
          const char* pipe = linkStart;
          while (pipe < linkEnd && *pipe != '|') pipe++;
          const char* textStart = (pipe < linkEnd) ? pipe + 1 : linkStart;

          for (const char* c = textStart; c < linkEnd && o < outMax - 1; c++) {
            if (c + 2 < linkEnd && c[0] == '\'' && c[1] == '\'' && c[2] == '\'') { c += 2; continue; }
            if (c + 1 < linkEnd && c[0] == '\'' && c[1] == '\'') { c += 1; continue; }
            out[o++] = *c;
          }
        }
        p = linkEnd + 2;
      } else {
        out[o++] = *p++;
      }
      continue;
    }

    // External link [url text]
    if (p < end && p[0] == '[' && (p + 1 >= end || p[1] != '[')) {
      const char* linkEnd = p + 1;
      while (linkEnd < end && *linkEnd != ']') linkEnd++;
      if (linkEnd < end) {
        const char* space = p + 1;
        while (space < linkEnd && *space != ' ') space++;
        if (space < linkEnd) {
          for (const char* c = space + 1; c < linkEnd && o < outMax - 1; c++) {
            out[o++] = *c;
          }
        }
        p = linkEnd + 1;
      } else {
        out[o++] = *p++;
      }
      continue;
    }

    // Template {{...}}
    if (p + 1 < end && p[0] == '{' && p[1] == '{') {
      int depth = 1;
      p += 2;
      while (p + 1 < end && depth > 0) {
        if (p[0] == '{' && p[1] == '{') { depth++; p += 2; }
        else if (p[0] == '}' && p[1] == '}') { depth--; p += 2; }
        else p++;
      }
      if (depth > 0) p = end;
      continue;
    }

    // HTML comment
    if (p + 3 < end && p[0] == '<' && p[1] == '!' && p[2] == '-' && p[3] == '-') {
      p += 4;
      while (p + 2 < end && !(p[0] == '-' && p[1] == '-' && p[2] == '>')) p++;
      if (p + 2 < end) p += 3;
      else p = end;
      continue;
    }

    // <ref> tags
    if (p + 3 < end && p[0] == '<' &&
        (p[1] == 'r' || p[1] == 'R') &&
        (p[2] == 'e' || p[2] == 'E') &&
        (p[3] == 'f' || p[3] == 'F')) {
      const char* tagEnd = p + 4;
      while (tagEnd < end && *tagEnd != '>') tagEnd++;
      if (tagEnd < end) {
        if (tagEnd > p && tagEnd[-1] == '/') {
          p = tagEnd + 1;
          continue;
        }
        const char* closeTag = tagEnd + 1;
        while (closeTag + 5 < end) {
          if (closeTag[0] == '<' && closeTag[1] == '/' &&
              (closeTag[2] == 'r' || closeTag[2] == 'R') &&
              (closeTag[3] == 'e' || closeTag[3] == 'E') &&
              (closeTag[4] == 'f' || closeTag[4] == 'F') &&
              closeTag[5] == '>') {
            break;
          }
          closeTag++;
        }
        if (closeTag + 5 < end) p = closeTag + 6;
        else p = end;
        continue;
      }
    }

    // <br> tags
    if (p + 2 < end && p[0] == '<' &&
        (p[1] == 'b' || p[1] == 'B') &&
        (p[2] == 'r' || p[2] == 'R')) {
      const char* tagEnd = p + 3;
      while (tagEnd < end && *tagEnd != '>') tagEnd++;
      if (tagEnd < end) {
        if (o > 0 && out[o-1] != '\n' && o < outMax - 1) out[o++] = '\n';
        p = tagEnd + 1;
        continue;
      }
    }

    // Other HTML tags
    if (p < end && p[0] == '<') {
      const char* tagEnd = p + 1;
      while (tagEnd < end && *tagEnd != '>') tagEnd++;
      if (tagEnd < end) {
        p = tagEnd + 1;
        continue;
      }
    }

    // HTML entities
    if (p < end && p[0] == '&') {
      if (p + 5 <= end && strncmp(p, "&amp;", 5) == 0) { out[o++] = '&'; p += 5; continue; }
      if (p + 4 <= end && strncmp(p, "&lt;", 4) == 0) { out[o++] = '<'; p += 4; continue; }
      if (p + 4 <= end && strncmp(p, "&gt;", 4) == 0) { out[o++] = '>'; p += 4; continue; }
      if (p + 6 <= end && strncmp(p, "&quot;", 6) == 0) { out[o++] = '"'; p += 6; continue; }
      if (p + 6 <= end && strncmp(p, "&nbsp;", 6) == 0) { out[o++] = ' '; p += 6; continue; }
      if (p + 5 <= end && strncmp(p, "&#39;", 5) == 0) { out[o++] = '\''; p += 5; continue; }
      if (p + 8 <= end) {
        if (strncmp(p, "&agrave;", 8) == 0) { out[o++] = (char)0xC3; out[o++] = (char)0xA0; p += 8; continue; }
        if (strncmp(p, "&egrave;", 8) == 0) { out[o++] = (char)0xC3; out[o++] = (char)0xA8; p += 8; continue; }
        if (strncmp(p, "&eacute;", 8) == 0) { out[o++] = (char)0xC3; out[o++] = (char)0xA9; p += 8; continue; }
        if (strncmp(p, "&igrave;", 8) == 0) { out[o++] = (char)0xC3; out[o++] = (char)0xAC; p += 8; continue; }
        if (strncmp(p, "&ograve;", 8) == 0) { out[o++] = (char)0xC3; out[o++] = (char)0xB2; p += 8; continue; }
        if (strncmp(p, "&ugrave;", 8) == 0) { out[o++] = (char)0xC3; out[o++] = (char)0xB9; p += 8; continue; }
        if (strncmp(p, "&ccedil;", 8) == 0) { out[o++] = (char)0xC3; out[o++] = (char)0xA7; p += 8; continue; }
      }
      if (p + 2 < end && p[1] == '#') {
        const char* numStart = p + 2;
        int val = 0;
        const char* numEnd = numStart;
        if (*numStart == 'x' || *numStart == 'X') {
          numStart++;
          while (numEnd < end && isxdigit((unsigned char)*numEnd)) {
            char c = *numEnd++;
            if (c <= '9') val = val * 16 + (c - '0');
            else val = val * 16 + ((c & 0xDF) - 'A' + 10);
          }
        } else {
          while (numEnd < end && isdigit((unsigned char)*numEnd)) {
            val = val * 10 + (*numEnd - '0');
            numEnd++;
          }
        }
        if (numEnd < end && *numEnd == ';') numEnd++;
        if (val < 0x80) { if (o < outMax - 1) out[o++] = (char)val; }
        else if (val < 0x800) { if (o < outMax - 2) { out[o++] = (char)(0xC0 | (val >> 6)); out[o++] = (char)(0x80 | (val & 0x3F)); } }
        else { if (o < outMax - 3) { out[o++] = (char)(0xE0 | (val >> 12)); out[o++] = (char)(0x80 | ((val >> 6) & 0x3F)); out[o++] = (char)(0x80 | (val & 0x3F)); } }
        p = numEnd;
        continue;
      }
      out[o++] = '&';
      p++;
      continue;
    }

    // Regular character
    out[o++] = *p++;
  }
}

// ======================================================================
// Wikitext Parser
// ======================================================================

size_t parseWikitext(const char* wtext, size_t wtextLen, char* out, size_t outSize) {
  size_t o = 0;
  const char* lineStart = wtext;
  const char* end = wtext + wtextLen;

  bool inTable = false;
  bool firstCellInRow = true;
  bool prevBlank = true;
  int olCounter[10] = {0};
  bool inSkipBlock = false;
  char skipTag[16] = "";

  auto ensureNewline = [&]() {
    if (o > 0 && out[o-1] != '\n' && o < outSize - 1) out[o++] = '\n';
  };

  auto ensureDoubleNewline = [&]() {
    ensureNewline();
    if (o > 1 && out[o-2] != '\n' && o < outSize - 1) out[o++] = '\n';
  };

  while (lineStart < end && o < outSize - 1) {
    const char* lineEnd = lineStart;
    while (lineEnd < end && *lineEnd != '\n') lineEnd++;
    size_t lineLen = lineEnd - lineStart;
    while (lineLen > 0 && lineStart[lineLen-1] == '\r') lineLen--;

    const char* content = lineStart;
    while (content < lineStart + lineLen && *content == ' ') content++;
    size_t contentLen = lineStart + lineLen - content;

    auto advance = [&]() {
      lineStart = (lineEnd < end) ? lineEnd + 1 : end;
    };

    if (inSkipBlock) {
      if (contentLen > 0) {
        std::string closeTag = std::string("</") + skipTag;
        if (startsWithCI(content, contentLen, closeTag.c_str())) {
          inSkipBlock = false;
          skipTag[0] = '\0';
        }
      }
      advance();
      continue;
    }

    if (contentLen > 0 && content[0] == '<') {
      const char* tagStart = content + 1;
      char tagName[16] = "";
      int ti = 0;
      while (tagStart < lineStart + lineLen && *tagStart != '>' && *tagStart != ' ' && *tagStart != '/' && ti < 15) {
        tagName[ti++] = tolower((unsigned char)*tagStart++);
      }
      tagName[ti] = '\0';

      if (strcmp(tagName, "gallery") == 0 || strcmp(tagName, "source") == 0 ||
          strcmp(tagName, "syntaxhighlight") == 0 || strcmp(tagName, "timeline") == 0 ||
          strcmp(tagName, "graph") == 0 || strcmp(tagName, "math") == 0 ||
          strcmp(tagName, "score") == 0 || strcmp(tagName, "hiero") == 0 ||
          strcmp(tagName, "pre") == 0 || strcmp(tagName, "nowiki") == 0 ||
          strcmp(tagName, "table") == 0 || strcmp(tagName, "div") == 0) {
        const char* tagEnd = content;
        while (tagEnd < lineStart + lineLen && *tagEnd != '>') tagEnd++;
        bool selfClosing = (tagEnd < lineStart + lineLen && tagEnd > content && tagEnd[-1] == '/');

        if (!selfClosing) {
          std::string closeTag = std::string("</") + tagName;
          bool foundClose = false;
          for (size_t i = 0; i + closeTag.length() <= contentLen; i++) {
            bool match = true;
            for (size_t j = 0; j < closeTag.length(); j++) {
              if (tolower((unsigned char)content[i+j]) != tolower((unsigned char)closeTag[j])) { match = false; break; }
            }
            if (match) { foundClose = true; break; }
          }
          if (!foundClose) {
            strncpy(skipTag, tagName, 15);
            skipTag[15] = '\0';
            inSkipBlock = true;
            advance();
            continue;
          }
        }
      }
    }

    // Table end |}
    if (contentLen >= 2 && content[0] == '|' && content[1] == '}') {
      ensureNewline();
      inTable = false;
      prevBlank = true;
      advance();
      continue;
    }

    // Table start {|
    if (contentLen >= 2 && content[0] == '{' && content[1] == '|') {
      ensureDoubleNewline();
      inTable = true;
      firstCellInRow = true;
      for (int i = 0; i < 10; i++) olCounter[i] = 0;
      advance();
      continue;
    }

    if (inTable) {
      if (contentLen >= 2 && content[0] == '|' && content[1] == '-') {
        ensureNewline();
        firstCellInRow = true;
        advance();
        continue;
      }

      if (contentLen >= 2 && content[0] == '|' && content[1] == '+') {
        ensureNewline();
        processInline(content + 2, contentLen - 2, out, o, outSize);
        ensureNewline();
        advance();
        continue;
      }

      if (contentLen > 0 && (content[0] == '|' || content[0] == '!')) {
        const char* cellPtr = content + 1;
        size_t remaining = contentLen - 1;

        while (remaining > 0 && o < outSize - 1) {
          if (remaining >= 2 && ((cellPtr[0] == '|' && cellPtr[1] == '|') || (cellPtr[0] == '!' && cellPtr[1] == '!'))) {
            cellPtr += 2;
            remaining -= 2;
          }

          if (!firstCellInRow) {
            if (o < outSize - 3) { out[o++] = ' '; out[o++] = '|'; out[o++] = ' '; }
          }
          firstCellInRow = false;

          const char* cellEnd = cellPtr;
          const char* lineLimit = content + contentLen;
          while (cellEnd < lineLimit) {
            if (cellEnd + 1 < lineLimit &&
                ((cellEnd[0] == '|' && cellEnd[1] == '|') ||
                 (cellEnd[0] == '!' && cellEnd[1] == '!'))) {
              break;
            }
            cellEnd++;
          }

          processInline(cellPtr, cellEnd - cellPtr, out, o, outSize);

          if (cellEnd >= lineLimit) break;
          cellPtr = cellEnd + 2;
          remaining = lineLimit - cellPtr;
        }

        ensureNewline();
        advance();
        continue;
      }
    }

    // Heading
    if (contentLen > 0 && content[0] == '=') {
      int level = 0;
      while (level < (int)contentLen && content[level] == '=') level++;
      if (level >= 2 && level <= 6) {
        const char* headingEnd = content + contentLen - 1;
        int closeLevel = 0;
        while (headingEnd > content + level && *headingEnd == '=') { closeLevel++; headingEnd--; }
        if (closeLevel >= 2) {
          size_t headingLen = headingEnd - (content + level) + 1;
          if (headingLen > 0) {
            ensureDoubleNewline();
            for (int i = 0; i < level - 1 && o < outSize - 1; i++) out[o++] = '#';
            if (o < outSize - 1) out[o++] = ' ';
            processInline(content + level, headingLen, out, o, outSize);
            ensureDoubleNewline();
            prevBlank = true;
            advance();
            continue;
          }
        }
      }
    }

    // Blank line
    if (contentLen == 0) {
      if (!prevBlank) ensureNewline();
      prevBlank = true;
      for (int i = 0; i < 10; i++) olCounter[i] = 0;
      advance();
      continue;
    }

    // Horizontal rule
    if (contentLen >= 4 && content[0] == '-' && content[1] == '-' && content[2] == '-' && content[3] == '-') {
      ensureDoubleNewline();
      if (o < outSize - 4) { out[o++] = '-'; out[o++] = '-'; out[o++] = '-'; }
      ensureDoubleNewline();
      prevBlank = true;
      advance();
      continue;
    }

    // List items
    if (contentLen > 0 && (content[0] == '*' || content[0] == '#' || content[0] == ';' || content[0] == ':')) {
      int depth = 0;
      const char* itemContent = content;
      while (itemContent < content + contentLen &&
             (*itemContent == '*' || *itemContent == '#' || *itemContent == ':' || *itemContent == ';')) {
        depth++;
        itemContent++;
      }

      ensureNewline();
      for (int i = 1; i < depth && o < outSize - 1; i++) out[o++] = ' ';

      if (content[0] == '*') {
        if (o < outSize - 3) { out[o++] = (char)0xE2; out[o++] = (char)0x80; out[o++] = (char)0xA2; out[o++] = ' '; }
      } else if (content[0] == '#') {
        if (depth <= 10) {
          olCounter[depth - 1]++;
          char num[8];
          snprintf(num, sizeof(num), "%d. ", olCounter[depth - 1]);
          for (const char* c = num; *c && o < outSize - 1; c++) out[o++] = *c;
        }
      } else if (content[0] == ';') {
        // Definition term
      } else {
        if (o < outSize - 3) { out[o++] = ' '; out[o++] = ' '; }
      }

      const char* defSep = itemContent;
      while (defSep < content + contentLen && *defSep != ':') defSep++;

      if (content[0] == ';' && defSep < content + contentLen) {
        processInline(itemContent, defSep - itemContent, out, o, outSize);
        if (o < outSize - 3) { out[o++] = ':'; out[o++] = ' '; }
        processInline(defSep + 1, content + contentLen - defSep - 1, out, o, outSize);
      } else {
        processInline(itemContent, content + contentLen - itemContent, out, o, outSize);
      }

      prevBlank = false;
      advance();
      continue;
    }

    // Redirects
    if (contentLen >= 7 &&
        (startsWithCI(content, contentLen, "#RINVIA") ||
         startsWithCI(content, contentLen, "#REDIRECT") ||
         startsWithCI(content, contentLen, "#Rinvia"))) {
      advance();
      continue;
    }

    // Magic words
    if (contentLen >= 2 && content[0] == '_' && content[contentLen-1] == '_') {
      advance();
      continue;
    }

    // Regular text line
    if (!prevBlank && o > 0 && out[o-1] != '\n') {
      if (o < outSize - 1) out[o++] = ' ';
    }
    processInline(content, contentLen, out, o, outSize);
    prevBlank = false;
    advance();
  }

  if (o < outSize) out[o] = '\0';
  else out[outSize - 1] = '\0';
  return o;
}

} // anonymous namespace

// ======================================================================
//  Reading Settings
// ======================================================================

void WikipediaActivity::cacheReadingSettings() {
  int px = fontSizeToPixels(SETTINGS.fontSize);
  readingLineHeight = std::max(px + 4, lineSpacingToLineHeight(SETTINGS.lineSpacing));
  if (SETTINGS.sdFontFamilyName[0] != '\0') {
    int id = sdFontSystem.resolveFontId(SETTINGS.sdFontFamilyName, SETTINGS.fontSize);
    if (id > 0) readingFontId = id;
  }
  if (readingFontId <= 0) readingFontId = builtinFontId(SETTINGS.fontFamily, px);
  readingMarginH = 8 + static_cast<int>(SETTINGS.screenMargin) * 3;
  readingMarginV = 4 + static_cast<int>(SETTINGS.screenMargin);
}

// ======================================================================
//  Buffer Management
// ======================================================================

char* WikipediaActivity::ensureBuffer() {
  if (!textBuffer) {
    textBuffer = std::make_unique<char[]>(TEXT_BUF_SIZE);
    textBuffer[0] = '\0';
    LOG_DBG("WIKI", "Allocated text buffer (%zu bytes)", TEXT_BUF_SIZE);
  }
  return textBuffer.get();
}

void WikipediaActivity::freeBuffer() {
  textBuffer.reset();
  textLength = 0;
  articlePageOffset = 0;
  g_articleFilePath.clear(); // Dimentica il percorso file se l'articolo era su SD
  LOG_DBG("WIKI", "Freed text buffer");
}

// ======================================================================
//  WiFi
// ======================================================================

void WikipediaActivity::wifiOff() {
  LOG_DBG("WIKI", "Turning off WiFi");
  WiFi.disconnect(false);
  delay(50);
  WiFi.mode(WIFI_OFF);
  delay(50);
}

// ======================================================================
//  Lifecycle
// ======================================================================

void WikipediaActivity::onEnter() {
  LOG_DBG("WIKI", "onEnter");
  Activity::onEnter();
  cacheReadingSettings();
  state = State::SEARCH_INPUT;
  selectedIndex = 0;
  searchInput.clear();
  searchResults.clear();
  textLength = 0;
  articlePageOffset = 0;
  errorMessage.clear();
  textBuffer.reset();
  g_articleFilePath.clear();
  loadHistory();
  loadCachedPages();
  requestUpdate();
}

void WikipediaActivity::onExit() {
  LOG_DBG("WIKI", "onExit");
  searchResults.clear();
  historyQueries.clear();
  cachedPageTitles.clear();
  freeBuffer();
  wifiOff();
}

// ======================================================================
//  Input Loop
// ======================================================================

void WikipediaActivity::loop() {
  static uint32_t lastLog = 0;
  uint32_t now = millis();
  if (now - lastLog > 2000) { LOG_DBG("WIKI", "loop state=%d", static_cast<int>(state)); lastLog = now; }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    switch (state) {
      case State::SEARCH_INPUT:
      case State::ERROR:
        finish(); break;
      case State::SEARCH_HISTORY:
      case State::CACHED_PAGES:
        state = State::SEARCH_INPUT; requestUpdate(); break;
      case State::SEARCH_RESULTS:
        state = State::SEARCH_INPUT; searchInput.clear(); searchResults.clear(); requestUpdate(); break;
      case State::ARTICLE_DISPLAY:
        if (!searchResults.empty()) {
          state = State::SEARCH_RESULTS;
        } else {
          state = State::SEARCH_INPUT;
        }
        requestUpdate(); break;
      case State::LOADING_ARTICLE:
      case State::LOADING_FULL_ARTICLE:
        freeBuffer(); state = State::SEARCH_INPUT; requestUpdate(); break;
      case State::FULL_ARTICLE:
        if (fromCache) {
          fromCache = false; freeBuffer(); state = State::CACHED_PAGES; requestUpdate();
        } else {
          freeBuffer(); state = State::ARTICLE_DISPLAY; requestUpdate();
        }
        break;
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    switch (state) {
      case State::SEARCH_INPUT:
        if (selectedIndex == 0) { if (!searchInput.empty()) performSearch(searchInput); else launchSearchKeyboard(); }
        else if (selectedIndex == 1) { state = State::SEARCH_HISTORY; selectedIndex = 0; loadHistory(); requestUpdate(); }
        else if (selectedIndex == 2) { state = State::CACHED_PAGES; selectedIndex = 0; loadCachedPages(); requestUpdate(); }
        break;
      case State::SEARCH_HISTORY:
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(historyQueries.size())) {
          if (mappedInput.getHeldTime() >= 1500) {
            LOG_DBG("WIKI", "Long-press: deleting history entry %d", selectedIndex);
            std::string toRemove = historyQueries[selectedIndex];
            String content;
            if (Storage.exists(HISTORY_FILE)) content = Storage.readFile(HISTORY_FILE);
            String searchStr = String(toRemove.c_str()) + "\n";
            int pos = content.indexOf(searchStr);
            if (pos >= 0) {
              content = content.substring(0, pos) + content.substring(pos + searchStr.length());
              Storage.writeFile(HISTORY_FILE, content);
            }
            loadHistory();
            if (selectedIndex >= static_cast<int>(historyQueries.size())) selectedIndex = std::max(0, static_cast<int>(historyQueries.size()) - 1);
            requestUpdate();
          } else {
            currentQuery = historyQueries[selectedIndex]; searchInput = currentQuery; performSearch(currentQuery);
          }
        }
        break;
      case State::CACHED_PAGES:
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(cachedPageTitles.size())) {
          if (mappedInput.getHeldTime() >= 1500 && !cachedPageTitles.empty()) {
            LOG_DBG("WIKI", "Long-press: deleting cached page %d", selectedIndex);
            std::string path = cachePathForTitle(cachedPageTitles[selectedIndex]);
            Storage.remove(path.c_str());
            loadCachedPages();
            if (selectedIndex >= static_cast<int>(cachedPageTitles.size())) selectedIndex = std::max(0, static_cast<int>(cachedPageTitles.size()) - 1);
            requestUpdate();
          } else {
            const std::string& title = cachedPageTitles[selectedIndex];
            if (loadCachedArticle(title)) {
              currentQuery = title; fromCache = true;
              state = State::FULL_ARTICLE; articlePageOffset = 0; requestUpdate();
            } else {
              currentQuery = title; searchInput = title; performSearch(title);
            }
          }
        }
        break;
      case State::SEARCH_RESULTS:
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(searchResults.size())) fetchArticleSummary();
        break;
      case State::ARTICLE_DISPLAY:
        LOG_DBG("WIKI", "Confirm on ARTICLE_DISPLAY — calling fetchFullArticle");
        fetchFullArticle();
        break;
      default: break;
    }
    return;
  }

  buttonNavigator.onNext([this] {
    if (state == State::SEARCH_INPUT) {
      int old = selectedIndex; selectedIndex = ButtonNavigator::nextIndex(selectedIndex, 3);
      if (old != selectedIndex) requestUpdate();
    } else if (state == State::SEARCH_HISTORY) {
      if (!historyQueries.empty()) { int old = selectedIndex; selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(historyQueries.size())); if (old != selectedIndex) requestUpdate(); }
    } else if (state == State::CACHED_PAGES) {
      if (!cachedPageTitles.empty()) { int old = selectedIndex; selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(cachedPageTitles.size())); if (old != selectedIndex) requestUpdate(); }
    } else if (state == State::SEARCH_RESULTS) {
      if (!searchResults.empty()) { int old = selectedIndex; selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(searchResults.size())); if (old != selectedIndex) requestUpdate(); }
    } else if (state == State::FULL_ARTICLE) {
      advancePage(1);
    }
  });

  buttonNavigator.onPrevious([this] {
    if (state == State::SEARCH_INPUT) {
      int old = selectedIndex; selectedIndex = ButtonNavigator::previousIndex(selectedIndex, 3);
      if (old != selectedIndex) requestUpdate();
    } else if (state == State::SEARCH_HISTORY) {
      if (!historyQueries.empty()) { int old = selectedIndex; selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(historyQueries.size())); if (old != selectedIndex) requestUpdate(); }
    } else if (state == State::CACHED_PAGES) {
      if (!cachedPageTitles.empty()) { int old = selectedIndex; selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(cachedPageTitles.size())); if (old != selectedIndex) requestUpdate(); }
    } else if (state == State::SEARCH_RESULTS) {
      if (!searchResults.empty()) { int old = selectedIndex; selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(searchResults.size())); if (old != selectedIndex) requestUpdate(); }
    } else if (state == State::FULL_ARTICLE) {
      advancePage(-1);
    }
  });
}

// ======================================================================
//  Rendering
// ======================================================================

void WikipediaActivity::render(RenderLock&&) {
  renderer.clearScreen();
  switch (state) {
    case State::SEARCH_INPUT:         renderSearchInput(); break;
    case State::SEARCH_HISTORY:       renderSearchHistory(); break;
    case State::CACHED_PAGES:         renderCachedPages(); break;
    case State::SEARCH_RESULTS:       renderResults(); break;
    case State::LOADING_ARTICLE:
    case State::LOADING_FULL_ARTICLE:
      renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight()/2, tr(STR_WIKIPEDIA_LOADING_ARTICLE));
      renderer.displayBuffer(); break;
    case State::ARTICLE_DISPLAY:      renderArticle(); break;
    case State::FULL_ARTICLE:         renderFullArticle(); break;
    case State::ERROR:                renderError(); break;
  }
}

void WikipediaActivity::renderSearchInput() {
  // Draw the Wikipedia icon (32x32) at the left of the header
  // Bitmap format: 1=white, 0=black, packed MSB-first, rotated 90°CCW by converter
  int iconX = 6;
  int iconY = 8;
  const uint8_t* iconData = WikipediaIcon;
  for (int row = 0; row < 32; row++) {
    for (int col = 0; col < 32; col++) {
      int byteIdx = row * 4 + col / 8;
      int bitIdx = col % 8;
      if (byteIdx < 128) {
        bool white = (iconData[byteIdx] & (0x80 >> bitIdx)) != 0;
        renderer.drawPixel(iconX + col, iconY + row, !white);
      }
    }
  }
  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_WIKIPEDIA));
  int pw = renderer.getScreenWidth();
  int hh = UITheme::getInstance().getMetrics().headerHeight;
  int ct = hh + 20;
  int bw = pw - 40;

  auto drawBtn = [&](int idx, int y, const char* label) {
    bool sel = selectedIndex == idx;
    if (sel) renderer.fillRect(20, y, bw, 36, 1);
    renderer.drawRect(20, y, bw, 36, 1);
    renderer.drawText(UI_10_FONT_ID, 28, y + 10, label, !sel);
  };

  const char* searchLbl = searchInput.empty() ? tr(STR_SEARCH_HINT) : searchInput.c_str();
  drawBtn(0, ct, searchLbl);
  drawBtn(1, ct + 46, tr(STR_RECENT_SEARCHES));
  int cacheCount = static_cast<int>(cachedPageTitles.size());
  char cacheLbl[64];
  snprintf(cacheLbl, sizeof(cacheLbl), "%s (%d)", tr(STR_CACHED_PAGES), cacheCount);
  drawBtn(2, ct + 92, cacheLbl);

  auto lb = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, lb.btn1, lb.btn2, lb.btn3, lb.btn4);
  renderer.displayBuffer();
}

void WikipediaActivity::renderSearchHistory() {
  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_RECENT_SEARCHES));
  auto layout = ListLayout::compute(renderer, true, false);
  if (historyQueries.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, layout.contentTop + layout.contentHeight/2, tr(STR_WIKIPEDIA_NO_RESULTS));
  } else {
    ListRenderHelper::drawList(renderer, layout.contentTop, layout.contentHeight,
                               static_cast<int>(historyQueries.size()), selectedIndex,
                               [this](int i) { return historyQueries[i]; }, nullptr, nullptr, nullptr, false, nullptr);
  }
  auto lb = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, lb.btn1, lb.btn2, lb.btn3, lb.btn4);
  renderer.displayBuffer();
}

void WikipediaActivity::renderCachedPages() {
  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_CACHED_PAGES));
  auto layout = ListLayout::compute(renderer, true, false);
  if (cachedPageTitles.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, layout.contentTop + layout.contentHeight/2, tr(STR_WIKIPEDIA_NO_RESULTS));
  } else {
    ListRenderHelper::drawList(renderer, layout.contentTop, layout.contentHeight,
                               static_cast<int>(cachedPageTitles.size()), selectedIndex,
                               [this](int i) { return cachedPageTitles[i]; }, nullptr, nullptr, nullptr, false, nullptr);
  }
  auto lb = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, lb.btn1, lb.btn2, lb.btn3, lb.btn4);
  renderer.displayBuffer();
}

void WikipediaActivity::renderResults() {
  auto layout = ListLayout::compute(renderer, true, false);
  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_WIKIPEDIA));
  int rc = static_cast<int>(searchResults.size());
  if (rc == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, layout.contentTop + layout.contentHeight/2, tr(STR_WIKIPEDIA_NO_RESULTS));
  } else {
    ListRenderHelper::drawList(renderer, layout.contentTop, layout.contentHeight, rc, selectedIndex,
                               [this](int i) { return searchResults[i]; }, nullptr, nullptr, nullptr, false, nullptr);
  }
  auto lb = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, lb.btn1, lb.btn2, lb.btn3, lb.btn4);
  renderer.displayBuffer();
}

void WikipediaActivity::renderArticle() {
  char* buf = ensureBuffer();
  if (!buf || textLength == 0) {
    LOG_DBG("WIKI", "renderArticle: empty buf=%p len=%zu", (void*)buf, textLength);
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight()/2, "Loading...");
    auto lb = mappedInput.mapLabels(tr(STR_BACK), nullptr, nullptr, nullptr);
    GUI.drawButtonHints(renderer, lb.btn1, nullptr, nullptr, nullptr);
    renderer.displayBuffer();
    return;
  }

  LOG_DBG("WIKI", "renderArticle: rendering %zu bytes", textLength);
  HeaderDateUtils::drawHeaderWithDate(renderer, currentQuery.c_str());
  int pw = renderer.getScreenWidth(), ph = renderer.getScreenHeight();
  int hh = UITheme::getInstance().getMetrics().headerHeight;
  int bh = UITheme::getInstance().getMetrics().buttonHintsHeight;
  int ct = hh + 4, ch = ph - ct - bh - 4, tw = pw - readingMarginH * 2;
  int fId = readingFontId > 0 ? readingFontId : UI_10_FONT_ID;

  const char* text = buf;
  int y = ct;

  while (*text && y + readingLineHeight <= ct + ch) {
    const char* nl = text;
    while (*nl && *nl != '\n') nl++;

    std::string seg(text, nl - text);
    auto wrapped = renderer.wrappedText(fId, seg.c_str(), tw, 1000);

    if (wrapped.empty()) {
      y += readingLineHeight / 2;
    }

    for (const auto& line : wrapped) {
      if (y + readingLineHeight > ct + ch) break;
      if (!line.empty()) {
        bool blank = true; for (char c : line) { if (c != ' ' && c != '\t') { blank = false; break; } }
        if (!blank) renderer.drawText(fId, readingMarginH, y, line.c_str(), true);
      }
      y += readingLineHeight;
    }

    if (*nl == '\n') nl++;
    text = nl;
  }

  if (y + readingLineHeight * 3 < ph - bh) {
    renderer.drawCenteredText(SMALL_FONT_ID, ph - bh - readingLineHeight, tr(STR_PRESS_CONFIRM_FULL_ARTICLE));
  }

  auto lb = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, lb.btn1, lb.btn2, lb.btn3, lb.btn4);
  renderer.displayBuffer();
}

void WikipediaActivity::renderFullArticle() {
  char* buf = ensureBuffer();
  size_t readLen = 0;

  // Se l'articolo è sulla SD, carica solo il blocco (chunk) attuale nel buffer
  if (!g_articleFilePath.empty()) {
    HalFile f;
    if (Storage.openFileForRead("WIKI", g_articleFilePath.c_str(), f)) {
      f.seek(articlePageOffset);
      readLen = f.read((uint8_t*)buf, TEXT_BUF_SIZE - 1);
      f.close();
    } else {
      g_articleFilePath.clear();
      textLength = 0;
    }
  } else if (textBuffer) {
    size_t available = (textLength > articlePageOffset) ? (textLength - articlePageOffset) : 0;
    readLen = std::min(available, TEXT_BUF_SIZE - 1);
    memcpy(buf, textBuffer.get() + articlePageOffset, readLen);
  }

  buf[readLen] = '\0';

  if (readLen == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight()/2, "No content");
    auto lb = mappedInput.mapLabels(tr(STR_BACK), nullptr, nullptr, nullptr);
    GUI.drawButtonHints(renderer, lb.btn1, nullptr, nullptr, nullptr);
    renderer.displayBuffer();
    return;
  }

  HeaderDateUtils::drawHeaderWithDate(renderer, currentQuery.c_str());
  int pw = renderer.getScreenWidth(), ph = renderer.getScreenHeight();
  int hh = UITheme::getInstance().getMetrics().headerHeight;
  int bh = UITheme::getInstance().getMetrics().buttonHintsHeight;
  int ct = hh + 4, ch = ph - ct - bh - 4, tw = pw - readingMarginH * 2;
  int fId = readingFontId > 0 ? readingFontId : UI_10_FONT_ID;

  const char* pos = buf;
  int y = ct;

  while (*pos && y + readingLineHeight <= ct + ch) {
    const char* nl = pos;
    while (*nl && *nl != '\n') nl++;

    std::string seg(pos, nl - pos);
    auto wrapped = renderer.wrappedText(fId, seg.c_str(), tw, 1000);

    if (wrapped.empty()) {
      y += readingLineHeight / 2;
    }

    for (const auto& wl : wrapped) {
      if (y + readingLineHeight > ct + ch) break;
      if (!wl.empty()) {
        bool blank = true; for (char c : wl) { if (c != ' ' && c != '\t') { blank = false; break; } }
        if (!blank) renderer.drawText(fId, readingMarginH, y, wl.c_str(), true);
      }
      y += readingLineHeight;
    }

    if (*nl == '\n') nl++;
    pos = nl;
  }

  renderer.drawCenteredText(SMALL_FONT_ID, ph - bh - readingLineHeight, "---");
  auto lb = mappedInput.mapLabels(tr(STR_BACK), nullptr, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, lb.btn1, lb.btn2, lb.btn3, lb.btn4);
  renderer.displayBuffer();
}

void WikipediaActivity::renderError() {
  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_WIKIPEDIA));
  renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight()/2 - 20, tr(STR_WIKIPEDIA_ERROR));
  if (!errorMessage.empty()) renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight()/2 + 10, errorMessage.c_str());
  auto lb = mappedInput.mapLabels(tr(STR_BACK), nullptr, nullptr, nullptr);
  GUI.drawButtonHints(renderer, lb.btn1, nullptr, nullptr, nullptr);
  renderer.displayBuffer();
}

// ======================================================================
//  Actions
// ======================================================================

void WikipediaActivity::performSearch(const std::string& query) {
  if (query.empty()) return;
  currentQuery = query; selectedIndex = 0;
  searchResults.clear();
  freeBuffer();

  NetworkMemory::prepareBeforeNetwork(renderer, "WIKI", "before_search");

  std::string encodedQuery = urlEncode(query);

  char url[512];
  snprintf(url, sizeof(url),
           "https://it.wikipedia.org/w/api.php?action=opensearch&search=%s&limit=10&namespace=0&format=json",
           encodedQuery.c_str());

  if (WiFi.status() != WL_CONNECTED) {
    NetworkMemory::restoreAfterNetwork(renderer, "WIKI", "search_wifi_check");
    startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, true, false),
                           [this](const ActivityResult& r) { onWifiSelectionComplete(!r.isCancelled); });
    return;
  }

  std::string response;
  bool ok = HttpDownloader::fetchUrl(url, response);
  NetworkMemory::restoreAfterNetwork(renderer, "WIKI", "after_search");

  if (!ok) { showError(tr(STR_ERROR_GENERAL_FAILURE)); return; }

  std::string titles[10];
  int found = parseOpensearchTitles(response.c_str(), titles, 10);
  searchResults.clear();
  for (int i = 0; i < found; i++) searchResults.push_back(titles[i]);

  if (searchResults.empty()) {
    state = State::ERROR; errorMessage = tr(STR_WIKIPEDIA_NO_RESULTS);
  } else {
    saveToHistory(query);
    if (searchResults.size() == 1) {
      LOG_DBG("WIKI", "Single result found, fetching summary directly");
      selectedIndex = 0;
      fetchArticleSummary();
    } else {
      state = State::SEARCH_RESULTS;
    }
  }
  requestUpdate();
}

void WikipediaActivity::fetchArticleSummary() {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(searchResults.size())) return;
  const std::string& title = searchResults[selectedIndex];

  if (loadCachedArticle(title)) {
    currentQuery = title;
    cacheReadingSettings();
    state = State::FULL_ARTICLE; articlePageOffset = 0; requestUpdate();
    return;
  }

  state = State::LOADING_ARTICLE;
  requestUpdate();
  freeBuffer();

  NetworkMemory::prepareBeforeNetwork(renderer, "WIKI", "before_article");
  std::string encodedTitle = urlEncodeForPath(title);
  char url[512];
  snprintf(url, sizeof(url), "https://it.wikipedia.org/api/rest_v1/page/summary/%s", encodedTitle.c_str());

  if (WiFi.status() != WL_CONNECTED) {
    NetworkMemory::restoreAfterNetwork(renderer, "WIKI", "summary_wifi_check");
    startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, true, false),
                           [this](const ActivityResult& r) {
                             if (!r.isCancelled && !currentQuery.empty()) fetchArticleSummary();
                             else showError(tr(STR_WIFI_CONN_FAILED));
                           });
    return;
  }

  std::string response;
  bool ok = HttpDownloader::fetchUrl(url, response);
  NetworkMemory::restoreAfterNetwork(renderer, "WIKI", "after_article");

  if (!ok) { showError(tr(STR_WIKIPEDIA_ERROR)); return; }

  char* buf = ensureBuffer();
  textLength = extractExtractField(response.c_str(), buf, TEXT_BUF_SIZE - 1);
  buf[textLength] = '\0';
  g_articleFilePath.clear(); // Il summary è in RAM, non su file

  if (textLength == 0) {
    LOG_DBG("WIKI", "Empty summary, fetching full article directly");
    currentQuery = title;
    fetchFullArticle();
    return;
  }

  currentQuery = title;
  state = State::ARTICLE_DISPLAY;
  requestUpdate();
}

void WikipediaActivity::fetchFullArticle() {
  LOG_DBG("WIKI", "fetchFullArticle: textLength=%zu", textLength);

  // Save the summary text as fallback in case download fails
  std::string fallbackText;
  if (textLength > 0 && textBuffer) {
    fallbackText.assign(textBuffer.get(), textLength);
  }

  state = State::LOADING_FULL_ARTICLE;
  requestUpdate();
  freeBuffer();

  // Ensure WiFi is connected
  NetworkMemory::prepareBeforeNetwork(renderer, "WIKI", "before_full");
  if (WiFi.status() != WL_CONNECTED) {
    NetworkMemory::restoreAfterNetwork(renderer, "WIKI", "full_wifi_check");
    startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, true, false),
                           [this](const ActivityResult& r) { if (!r.isCancelled) fetchFullArticle(); else showError(tr(STR_WIFI_CONN_FAILED)); });
    return;
  }

  // Build URL-encoded title for Wikipedia API
  std::string titleForUrl = currentQuery;
  for (auto& c : titleForUrl) { if (c == ' ') c = '_'; }
  std::string encodedTitle = urlEncode(titleForUrl);

  std::string rawPath = std::string(CACHE_DIR) + "/raw_" + sanitizeFilename(currentQuery) + ".html";
  Storage.mkdir(CACHE_DIR);
  LOG_DBG("WIKI", "Streaming mobile-html to SD: %s", rawPath.c_str());

  char url[512];
  snprintf(url, sizeof(url),
           "https://it.wikipedia.org/api/rest_v1/page/mobile-html/%s",
           encodedTitle.c_str());

  // Custom streaming download: read from HTTP stream, write chunks to SD file
  std::unique_ptr<NetworkClient> httpClient;
  auto* secClient = new NetworkClientSecure();
  secClient->setInsecure();
  secClient->setTimeout(60);
  httpClient.reset(secClient);

  HTTPClient http;
  http.begin(*httpClient, url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("User-Agent", "CrossPoint-ESP32/1.0");

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    LOG_ERR("WIKI", "HTTP failed: %d", httpCode);
    http.end();
    if (!fallbackText.empty()) {
      char* buf = ensureBuffer();
      size_t len = std::min(fallbackText.size(), static_cast<size_t>(TEXT_BUF_SIZE - 1));
      memcpy(buf, fallbackText.c_str(), len);
      textLength = len; buf[textLength] = '\0';
      cacheArticle(currentQuery);
      state = State::FULL_ARTICLE; articlePageOffset = 0; requestUpdate();
    } else { showError(tr(STR_WIKIPEDIA_ERROR)); }
    return;
  }

  // Stream response directly to SD file, chunk by chunk
  HalFile sdFile;
  if (!Storage.openFileForWrite("WIKI", rawPath.c_str(), sdFile)) {
    LOG_ERR("WIKI", "Failed to open SD file for writing");
    http.end();
    showError(tr(STR_WIKIPEDIA_ERROR));
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {
    LOG_ERR("WIKI", "Failed to get HTTP stream");
    sdFile.close();
    http.end();
    showError(tr(STR_WIKIPEDIA_ERROR));
    return;
  }

  uint8_t chunkBuf[512];
  size_t totalSaved = 0;
  unsigned long lastChunk = millis();
  bool streamOk = true;
  const unsigned long GRACEFUL_IDLE = 10000;
  const unsigned long STALL_TIMEOUT = 60000;

  while (http.connected() || stream->available()) {
    if (stream->available() == 0) {
      unsigned long idle = millis() - lastChunk;
      if (idle > GRACEFUL_IDLE && totalSaved > 1000) {
        LOG_DBG("WIKI", "Idle %lu ms, download assumed complete at %zu bytes", idle, totalSaved);
        break;
      }
      if (idle > STALL_TIMEOUT) {
        LOG_ERR("WIKI", "Stream stalled after %zu bytes (%lu ms idle)", totalSaved, idle);
        streamOk = false;
        break;
      }
      delay(5);
      continue;
    }
    int got = stream->read(chunkBuf, sizeof(chunkBuf));
    if (got > 0) {
      size_t wrote = sdFile.write(chunkBuf, got);
      if (wrote != static_cast<size_t>(got)) {
        LOG_ERR("WIKI", "SD write failed at %zu bytes", totalSaved);
        streamOk = false;
        break;
      }
      totalSaved += wrote;
      lastChunk = millis();
    } else if (got < 0) {
      LOG_ERR("WIKI", "Stream read error: %d at %zu bytes", got, totalSaved);
      streamOk = false;
      break;
    }
  }

  sdFile.close();

  if (!streamOk || totalSaved == 0) {
    http.end();
    NetworkMemory::restoreAfterNetwork(renderer, "WIKI", "after_full");
    Storage.remove(rawPath.c_str());
    LOG_DBG("WIKI", "Download incomplete (%zu bytes), using fallback", totalSaved);
    if (!fallbackText.empty()) {
      char* buf = ensureBuffer();
      size_t len = std::min(fallbackText.size(), static_cast<size_t>(TEXT_BUF_SIZE - 1));
      memcpy(buf, fallbackText.c_str(), len);
      textLength = len; buf[textLength] = '\0';
      state = State::FULL_ARTICLE; articlePageOffset = 0; requestUpdate();
    } else { showError(tr(STR_WIKIPEDIA_ERROR)); }
    return;
  }

  http.end();
  LOG_DBG("WIKI", "Streamed %zu bytes to SD, starting conversion...", totalSaved);

  // Convert HTML directly into the final cache file .wiki
  LOG_DBG("WIKI", "Converting HTML to text...");
  HalFile inFile;
  if (!Storage.openFileForRead("WIKI", rawPath, inFile)) {
    LOG_ERR("WIKI", "Failed to open raw HTML for conversion");
    showError(tr(STR_WIKIPEDIA_ERROR));
    return;
  }

  std::string cachePath = cachePathForTitle(currentQuery);
  if (Storage.exists(cachePath.c_str())) Storage.remove(cachePath.c_str());

  HtmlToTxt converter;
  bool convOk = converter.convert(inFile, cachePath.c_str()); // Converti e salva direttamente in .wiki
  inFile.close();
  Storage.remove(rawPath.c_str());

  if (!convOk) {
    LOG_ERR("WIKI", "HTML conversion failed");
    Storage.remove(cachePath.c_str());
    if (!fallbackText.empty()) {
      char* buf = ensureBuffer();
      size_t len = std::min(fallbackText.size(), static_cast<size_t>(TEXT_BUF_SIZE - 1));
      memcpy(buf, fallbackText.c_str(), len);
      textLength = len; buf[textLength] = '\0';
      state = State::FULL_ARTICLE; articlePageOffset = 0; requestUpdate();
    } else { showError(tr(STR_WIKIPEDIA_ERROR)); }
    return;
  }

  LOG_DBG("WIKI", "Conversion done. Article saved to SD cache.");

  // Imposta l'articolo per la lettura da SD (nessuna allocazione in RAM)
  g_articleFilePath = cachePath;
  
  HalFile sizeFile;
  if (Storage.openFileForRead("WIKI", g_articleFilePath.c_str(), sizeFile)) {
    textLength = sizeFile.size();
    sizeFile.close();
  } else {
    textLength = 0;
  }
  
  textBuffer.reset(); // Libera la RAM
  articlePageOffset = 0;

  if (textLength == 0) {
    if (!fallbackText.empty()) {
      char* buf = ensureBuffer();
      size_t len = std::min(fallbackText.size(), static_cast<size_t>(TEXT_BUF_SIZE - 1));
      memcpy(buf, fallbackText.c_str(), len);
      textLength = len; buf[textLength] = '\0';
      g_articleFilePath.clear();
    } else {
      showError(tr(STR_WIKIPEDIA_ERROR));
      return;
    }
  }

  loadCachedPages();
  LOG_DBG("WIKI", "Full article ready: %zu bytes (streamed from SD)", textLength);
  state = State::FULL_ARTICLE;
  requestUpdate();
}

void WikipediaActivity::launchSearchKeyboard() {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_WIKIPEDIA), searchInput, 128),
      [this](const ActivityResult& r) {
        if (!r.isCancelled) { searchInput = std::get<KeyboardResult>(r.data).text; performSearch(searchInput); }
      });
}

void WikipediaActivity::onWifiSelectionComplete(bool connected) {
  if (connected && !currentQuery.empty()) performSearch(currentQuery);
  else showError(tr(STR_WIFI_CONN_FAILED));
}

void WikipediaActivity::goBackToResults() {
  freeBuffer(); state = State::SEARCH_RESULTS; requestUpdate();
}

void WikipediaActivity::showError(const std::string& msg) {
  freeBuffer(); errorMessage = msg; state = State::ERROR; requestUpdate();
}

void WikipediaActivity::advancePage(int dir) {
  if (textLength == 0 && g_articleFilePath.empty()) return;

  int pw = renderer.getScreenWidth(), ph = renderer.getScreenHeight();
  int hh = UITheme::getInstance().getMetrics().headerHeight;
  int bh = UITheme::getInstance().getMetrics().buttonHintsHeight;
  int ct = hh + 4, ch = ph - ct - bh - 4, tw = pw - readingMarginH * 2;
  int fId = readingFontId > 0 ? readingFontId : UI_10_FONT_ID;

  char* buf = ensureBuffer();

  // Lambda per leggere un blocco di testo (da file o da RAM)
  auto readChunk = [&](size_t offset) -> size_t {
    if (!g_articleFilePath.empty()) {
      HalFile f;
      if (Storage.openFileForRead("WIKI", g_articleFilePath.c_str(), f)) {
        f.seek(offset);
        size_t r = f.read((uint8_t*)buf, TEXT_BUF_SIZE - 1);
        f.close();
        buf[r] = '\0';
        return r;
      }
      return 0;
    } else if (textBuffer) {
      size_t available = (textLength > offset) ? (textLength - offset) : 0;
      size_t r = std::min(available, TEXT_BUF_SIZE - 1);
      memcpy(buf, textBuffer.get() + offset, r);
      buf[r] = '\0';
      return r;
    }
    return 0;
  };

  if (dir > 0) {
    size_t readLen = readChunk(articlePageOffset);
    if (readLen == 0) return;

    const char* pos = buf;
    int y = ct;
    while (*pos && y + readingLineHeight <= ct + ch) {
      const char* nl = pos;
      while (*nl && *nl != '\n') nl++;
      std::string seg(pos, nl - pos);
      auto wrapped = renderer.wrappedText(fId, seg.c_str(), tw, 1000);
      if (wrapped.empty()) y += readingLineHeight / 2;
      for (const auto& wl : wrapped) {
        if (y + readingLineHeight > ct + ch) break;
        y += readingLineHeight;
      }
      if (*nl == '\n') nl++;
      pos = nl;
    }

    size_t consumed = pos - buf;
    if (consumed == 0 && readLen > 0) consumed = 1; // Evita blocco se riga lunghissima

    size_t newOffset = articlePageOffset + consumed;
    if (newOffset < textLength) {
      articlePageOffset = newOffset;
      requestUpdate();
    } else if (articlePageOffset < textLength) {
      articlePageOffset = textLength;
      requestUpdate();
    }
  } else {
    if (articlePageOffset == 0) return;

    int avgCharWidth = (fId == UI_10_FONT_ID) ? 8 : 10;
    int charsPerLine = tw / avgCharWidth;
    int linesPerPage = ch / readingLineHeight;
    int estimatedCharsPerPage = charsPerLine * linesPerPage;

    size_t searchStart = (articlePageOffset > (size_t)(estimatedCharsPerPage * 1.5))
                         ? articlePageOffset - (size_t)(estimatedCharsPerPage * 1.5) : 0;

    size_t lastPageStart = 0;
    size_t currentPos = searchStart;

    while (currentPos < articlePageOffset) {
      lastPageStart = currentPos;
      size_t readLen = readChunk(currentPos);
      if (readLen == 0) break;

      const char* pos = buf;
      int y = ct;
      while (*pos && y + readingLineHeight <= ct + ch) {
        const char* nl = pos;
        while (*nl && *nl != '\n') nl++;
        std::string seg(pos, nl - pos);
        auto wrapped = renderer.wrappedText(fId, seg.c_str(), tw, 1000);
        if (wrapped.empty()) y += readingLineHeight / 2;
        for (const auto& wl : wrapped) {
          if (y + readingLineHeight > ct + ch) break;
          y += readingLineHeight;
        }
        if (*nl == '\n') nl++;
        pos = nl;
      }

      size_t consumed = pos - buf;
      if (consumed == 0) break;
      size_t newPos = currentPos + consumed;

      if (newPos >= articlePageOffset) break;
      currentPos = newPos;
    }

    if (lastPageStart != articlePageOffset) {
      articlePageOffset = lastPageStart;
      requestUpdate();
    }
  }
}

int WikipediaActivity::estimateCharsPerPage() {
  int pw = renderer.getScreenWidth(), ph = renderer.getScreenHeight();
  int hh = UITheme::getInstance().getMetrics().headerHeight;
  int bh = UITheme::getInstance().getMetrics().buttonHintsHeight;
  int ch = ph - hh - bh - 8;
  int avgCharWidth = (readingFontId == UI_10_FONT_ID) ? 8 : 10;
  int charsPerLine = pw / avgCharWidth;
  int linesPerPage = ch / readingLineHeight;
  return std::max(200, charsPerLine * linesPerPage);
}

// ======================================================================
//  Search History
// ======================================================================

void WikipediaActivity::loadHistory() {
  historyQueries.clear();
  if (!Storage.exists(HISTORY_FILE)) return;
  String content = Storage.readFile(HISTORY_FILE);
  int start = 0;
  while (start < static_cast<int>(content.length())) {
    int end = content.indexOf('\n', start);
    if (end < 0) end = content.length();
    if (end > start) {
      std::string line = content.substring(start, end).c_str();
      while (!line.empty() && (line.back()=='\r' || line.back()=='\n' || line.back()==' ')) line.pop_back();
      if (!line.empty()) historyQueries.push_back(line);
    }
    start = end + 1;
  }
}

void WikipediaActivity::saveToHistory(const std::string& query) {
  String content;
  if (Storage.exists(HISTORY_FILE)) content = Storage.readFile(HISTORY_FILE);
  String newEntry = String(query.c_str()) + "\n";
  int dupPos = content.indexOf(newEntry);
  if (dupPos >= 0) {
    int dupEnd = content.indexOf('\n', dupPos + 1);
    if (dupEnd < 0) dupEnd = content.length();
    content = content.substring(0, dupPos) + content.substring(dupEnd + 1);
  }
  content = newEntry + content;
  int lineCount = 0, trimPos = 0;
  for (int i = 0; i < static_cast<int>(content.length()) && lineCount <= MAX_HISTORY; i++) {
    if (content[i] == '\n') { lineCount++; if (lineCount == MAX_HISTORY) { trimPos = i + 1; break; } }
  }
  if (trimPos > 0) content = content.substring(0, trimPos);
  Storage.writeFile(HISTORY_FILE, content);
  loadHistory();
}

// ======================================================================
//  Article Cache
// ======================================================================

std::string WikipediaActivity::sanitizeFilename(const std::string& s) {
  std::string r = s;
  for (auto& c : r) {
    if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
      c = '_';
    }
  }
  if (r.length() > 50) r = r.substr(0, 50);
  return r;
}

std::string WikipediaActivity::cachePathForTitle(const std::string& title) {
  return std::string(CACHE_DIR) + "/" + sanitizeFilename(title) + CACHE_EXT;
}

bool WikipediaActivity::cacheArticle(const std::string& title) {
  if (!textBuffer || textLength == 0) return false;
  Storage.mkdir(CACHE_DIR);
  std::string path = cachePathForTitle(title);
  String s(textBuffer.get(), textLength);
  bool ok = Storage.writeFile(path.c_str(), s);
  if (ok) {
    LOG_DBG("WIKI", "Cached article: %s (%zu bytes)", path.c_str(), textLength);
    loadCachedPages();
  }
  return ok;
}

bool WikipediaActivity::loadCachedArticle(const std::string& title) {
  std::string path = cachePathForTitle(title);
  if (!Storage.exists(path.c_str())) return false;

  // Controlla la dimensione del file
  HalFile f;
  if (!Storage.openFileForRead("WIKI", path.c_str(), f)) return false;
  size_t fileSize = f.size();
  f.close();

  if (fileSize == 0) return false;

  // Se il file è grande, non caricarlo in RAM. Verrà letto a chunk.
  if (fileSize > TEXT_BUF_SIZE - 1) {
    g_articleFilePath = path;
    textLength = fileSize;
    textBuffer.reset(); // Libera la RAM
    articlePageOffset = 0;
    LOG_DBG("WIKI", "Loaded large cached article: %s (%zu bytes on SD)", path.c_str(), textLength);
    return true;
  }

  // Se il file è piccolo, caricalo tutto in RAM
  String content = Storage.readFile(path.c_str());
  if (content.length() == 0) return false;

  char* buf = ensureBuffer();
  size_t maxCopy = std::min(static_cast<size_t>(content.length()), TEXT_BUF_SIZE - 1);
  memcpy(buf, content.c_str(), maxCopy);
  textLength = maxCopy;
  buf[textLength] = '\0';
  g_articleFilePath.clear();
  articlePageOffset = 0;
  LOG_DBG("WIKI", "Loaded cached article: %s (%zu bytes in RAM)", path.c_str(), textLength);
  return true;
}

void WikipediaActivity::loadCachedPages() {
  cachedPageTitles.clear();
  Storage.mkdir(CACHE_DIR);
  auto files = Storage.listFiles(CACHE_DIR, 100);
  for (auto& f : files) {
    std::string name = f.c_str();
    size_t extPos = name.rfind(CACHE_EXT);
    if (extPos != std::string::npos) {
      name = name.substr(0, extPos);
    }
    if (!name.empty()) {
      std::replace(name.begin(), name.end(), '_', ' ');
      cachedPageTitles.push_back(name);
    }
  }
  std::sort(cachedPageTitles.begin(), cachedPageTitles.end());
  LOG_DBG("WIKI", "Loaded %zu cached pages", cachedPageTitles.size());
}
