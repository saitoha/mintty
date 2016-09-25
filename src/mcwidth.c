// xcwidth.c (part of mintty)
// Copyright 2009-10 Andy Koppe
// Adapted from code by Markus Kuhn.
// Licensed under the terms of the GNU General Public License v3 or later.

#include "charset.h"

typedef struct {
  xchar first;
  xchar last;
} interval;

/* auxiliary function for binary search in interval table */
static bool
bisearch(xchar c, const interval table[], int len)
{
  int min = 0, max = len - 1;

  if (c < table[0].first || c > table[max].last)
    return false;
  while (max >= min) {
    int mid = (min + max) / 2;
    if (c > table[mid].last)
      min = mid + 1;
    else if (c < table[mid].first)
      max = mid - 1;
    else
      return true;
  }

  return false;
}


/* sorted list of non-overlapping intervals of East Asian wide characters */
static const interval wide[] = {
  { 0x1100, 0x115F }, /* Hangul Jamo init. consonants */
  { 0x2329, 0x232A }, /* angle brackets */
  { 0x2E80, 0x303E }, /* CJK symbols and punctuation */
  { 0x3040, 0xA4CF }, /* CJK ... Yi */
  { 0xA960, 0xA97F }, /* Hangul Jamo Extended-A */
  { 0xAC00, 0xD7A3 }, /* Hangul Syllables */
  { 0xF900, 0xFAFF }, /* CJK Compatibility Ideographs */
  { 0xFE10, 0xFE19 }, /* Vertical forms */
  { 0xFE30, 0xFE6F }, /* CJK Compatibility Forms */
  { 0xFF00, 0xFF60 }, /* Fullwidth Forms */
  { 0xFFE0, 0xFFE6 }, /* fullwidth symbols */
  { 0x1B000, 0x1B0FF }, /* Kana Supplement */
  { 0x1F200, 0x1F2FF }, /* Enclosed Ideographic Supplement */
  { 0x20000, 0x2FFFD }, /* CJK Extensions and Supplement */
  { 0x30000, 0x3FFFD }
};

#if !HAS_LOCALES

/*
 * This is an implementation of wcwidth() (defined in IEEE Std 1002.1-2001)
 * for Unicode.
 *
 * http://www.opengroup.org/onlinepubs/007904975/functions/wcwidth.html
 *
 * In fixed-width output devices, Latin characters all occupy a single
 * "cell" position of equal width, whereas ideographic CJK characters
 * occupy two such cells. Interoperability between terminal-line
 * applications and (teletype-style) character terminals using the
 * UTF-8 encoding requires agreement on which character should advance
 * the cursor by how many cell positions. No established formal
 * standards exist at present on which Unicode character shall occupy
 * how many cell positions on character terminals. These routines are
 * a first attempt of defining such behavior based on simple rules
 * applied to data provided by the Unicode Consortium.
 *
 * For some graphical characters, the Unicode standard explicitly
 * defines a character-cell width via the definition of the East Asian
 * FullWidth (F), Wide (W), Half-width (H), and Narrow (Na) classes.
 * In all these cases, there is no ambiguity about which width a
 * terminal shall use. For characters in the East Asian Ambiguous (A)
 * class, the width choice depends purely on a preference of backward
 * compatibility with either historic CJK or Western practice.
 * Choosing single-width for these characters is easy to justify as
 * the appropriate long-term solution, as the CJK practice of
 * displaying these characters as double-width comes from historic
 * implementation simplicity (8-bit encoded characters were displayed
 * single-width and 16-bit ones double-width, even for Greek,
 * Cyrillic, etc.) and not any typographic considerations.
 *
 * Much less clear is the choice of width for the Not East Asian
 * (Neutral) class. Existing practice does not dictate a width for any
 * of these characters. It would nevertheless make sense
 * typographically to allocate two character cells to characters such
 * as for instance EM SPACE or VOLUME INTEGRAL, which cannot be
 * represented adequately with a single-width glyph. The following
 * routines at present merely assign a single-cell width to all
 * neutral characters, in the interest of simplicity. This is not
 * entirely satisfactory and should be reconsidered before
 * establishing a formal standard in this area. At the moment, the
 * decision which Not East Asian (Neutral) characters should be
 * represented by double-width glyphs cannot yet be answered by
 * applying a simple rule from the Unicode database content. Setting
 * up a proper standard for the behavior of UTF-8 character terminals
 * will require a careful analysis not only of each Unicode character,
 * but also of each presentation form, something the author of these
 * routines has avoided to do so far.
 *
 * http://www.unicode.org/unicode/reports/tr11/
 *
 * Markus Kuhn -- 2007-05-26 (Unicode 5.0)
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose and without fee is hereby granted. The author
 * disclaims all warranties with regard to this software.
 *
 * Latest version: http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
 */


/* The following function defines the column width of an ISO 10646
 * character as follows:
 *
 *    - The null character (U+0000) has a column width of 0.
 *
 *    - Other C0/C1 control characters and DEL will lead to a return
 *      value of -1.
 *
 *    - Non-spacing and enclosing combining characters (general
 *      category code Mn or Me in the Unicode database) have a
 *      column width of 0.
 *
 *    - SOFT HYPHEN (U+00AD) has a column width of 1.
 *
 *    - Other format characters (general category code Cf in the Unicode
 *      database) and ZERO WIDTH SPACE (U+200B) have a column width of 0.
 *
 *    - Hangul Jamo medial vowels and final consonants (U+1160-U+11FF)
 *      have a column width of 0.
 *
 *    - Spacing characters in the East Asian Wide (W) or East Asian
 *      Full-width (F) category as defined in Unicode Technical
 *      Report #11 have a column width of 2.
 *
 *    - All remaining characters (including all printable
 *      ISO 8859-1 and WGL4 characters, Unicode control characters,
 *      etc.) have a column width of 1.
 *
 * This implementation assumes that xchar (rather than wchar_t) characters 
 * are encoded in ISO 10646.
 */

/* sorted list of non-overlapping intervals of non-spacing characters
   with Hangul fix: U+D7B0...U+D7C6 , U+D7CB...U+D7FB
   generated by:
 uniset +cat=Me +cat=Mn +cat=Cf -00AD +1160-11FF +200B +D7B0-D7C6 +D7CB-D7FB c
 */
static const interval combining[] =
#include "combining.t"

/* sorted list of non-overlapping intervals of
   East Asian Ambiguous characters, generated by
     uniset +WIDTH-A -cat=Me -cat=Mn -cat=Cf c
 */
static const interval ambiguous[] =
#include "ambiguous.t"

int
xcwidth(xchar c)
{
  /* null character */
  if (c == 0)
    return 0;

  /* printable ASCII characters */
  if (c >= 0x20 && c < 0x7f)
    return 1;

  /* control characters */
  if (c < 0xa0)
    return -1;

  /* non-spacing characters */
  if (bisearch(c, combining, lengthof(combining)))
    return 0;

  /* CJK ambiguous characters */
  if (bisearch(c, ambiguous, lengthof(ambiguous)))
    return font_ambig_wide + 1;

  /* wide characters */
  if (bisearch(c, wide, lengthof(wide)))
    return 2;

  /* anything else */
  return 1;
}

#endif

bool
widerange(xchar c)
{
  return bisearch(c, wide, lengthof(wide));
}

static const interval indic[] = {
#include "indicwide.t"
};

bool
indicwide(xchar c)
{
  return bisearch(c, indic, lengthof(indic));
}

static const interval extra[] = {
#include "longchars.t"
};

bool
extrawide(xchar c)
{
  return bisearch(c, extra, lengthof(extra));
}

static const interval combdouble[] =
#include "combdouble.t"

bool
combiningdouble(xchar c)
{
  return bisearch(c, combdouble, lengthof(combdouble));
}

