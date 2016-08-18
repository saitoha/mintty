// sixel.c (part of mintty)
// originally written by kmiya@cluti (https://github.com/saitoha/sixel/blob/master/fromsixel.c)
// Licensed under the terms of the GNU General Public License v3 or later.

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>   /* isdigit */
#include <string.h>  /* memcpy */
#include <windows.h>

#include "term.h"
#include "winpriv.h"
#include "winimg.h"

bool
winimg_new(imglist **ppimg, unsigned char *pixels,
           int left, int top, int width, int height,
           int pixelwidth, int pixelheight)
{
  imglist *img;

  img = (imglist *)malloc(sizeof(imglist));
  if (!img)
    return false;

  img->pixels = pixels;
  img->hdc = NULL;
  img->hbmp = NULL;
  img->left = left;
  img->top = top;
  img->width = width;
  img->height = height;
  img->pixelwidth = pixelwidth;
  img->pixelheight = pixelheight;
  img->next = NULL;
  img->refresh = 1;

  *ppimg = img;

  return true;
}

void
winimg_lazyinit(imglist *img)
{
  BITMAPINFO bmpinfo;
  unsigned char *pixels;
  HDC dc;
 
  if (img->hdc)
    return;

  dc = GetDC(wnd);

  bmpinfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmpinfo.bmiHeader.biWidth = img->pixelwidth;
  bmpinfo.bmiHeader.biHeight = - img->pixelheight;
  bmpinfo.bmiHeader.biPlanes = 1;
  bmpinfo.bmiHeader.biBitCount = 32;
  bmpinfo.bmiHeader.biCompression = BI_RGB;
  img->hdc = CreateCompatibleDC(dc);
  img->hbmp = CreateDIBSection(dc, &bmpinfo, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
  SelectObject(img->hdc, img->hbmp);
  CopyMemory(pixels, img->pixels, img->pixelwidth * img->pixelheight * 4);
  free(img->pixels);
  img->pixels = pixels;

  ReleaseDC(wnd, dc);
}

int
winimg_getleft(imglist *img)
{
  return img->left;
}

int
winimg_gettop(imglist *img)
{
  return img->top - term.virtuallines - term.disptop;
}

void
winimg_destroy(imglist *img)
{
  if (img->hdc) {
    DeleteDC(img->hdc);
    DeleteObject(img->hbmp);
  } else {
    free(img->pixels);
  }
  free(img);
}

void
winimgs_clear(void)
{
  imglist *img, *prev;

  for (img = term.imgs.first; img; ) {
    winimg_destroy(img);
    prev = img;
    img = img->next;
    free(prev);
  }

  for (img = term.imgs.altfirst; img; ) {
    winimg_destroy(img);
    prev = img;
    img = img->next;
    free(prev);
  }

  term.imgs.first = NULL;
  term.imgs.last = NULL;
  term.imgs.first = NULL;
  term.imgs.last = NULL;
}

void
winimg_paint(void)
{
  imglist *img;
  imglist *prev = NULL;
  int left, top;
  int x, y;
  termchar *tchar, *dchar;
  bool update_flag;
  int n = 0;
  HDC dc = GetDC(wnd);

  for (img = term.imgs.first; img; ++n) {
    // if the image is out of scrollbackjjj
    if (img->top + img->height - term.virtuallines < - term.sblines) {
      if (img == term.imgs.first)
        term.imgs.first = img->next;
      if (img == term.imgs.last)
        term.imgs.last = prev;
      if (prev)
        prev->next = img->next;
      prev = img;
      img = img->next;
      winimg_destroy(prev);
    } else {
      if (img->hdc == NULL)
        winimg_lazyinit(img);
      left = winimg_getleft(img);
      top = winimg_gettop(img);

      if (top + img->height > 0) {
        if (!img->refresh) {
          for (y = max(0, top); y < min(top + img->height, term.rows); ++y) {
            for (x = left; x < min(left + img->width, term.cols); ++x) {
              tchar = &term.lines[y]->chars[x];
              dchar = &term.displines[y]->chars[x];
              update_flag = false;
              if (term.disptop >= 0 && !(tchar->attr.attr & TATTR_SIXEL)) {
                update_flag = true;
                tchar->attr.attr |= TATTR_SIXEL;
              }
              if (dchar->attr.attr & (TATTR_RESULT| TATTR_CURRESULT))
                update_flag = true;
              if (update_flag)
                BitBlt(img->hdc, (x - left) * cell_width, (y - top) * cell_height,
                       cell_width, cell_height,
                       dc, x * cell_width + PADDING, y * cell_height + PADDING, SRCCOPY);
            }
          }
        }
        img->refresh = 0;
        StretchBlt(dc, left * cell_width + PADDING, top * cell_height + PADDING,
                   img->width * cell_width, img->height * cell_height, img->hdc,
                   0, 0, img->pixelwidth, img->pixelheight, SRCCOPY);
      }
    }
    prev = img;
    img = img->next;
  }
  ReleaseDC(wnd, dc);
}
