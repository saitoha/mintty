// winimg.c (part of mintty)
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
  img->refresh = true;

  *ppimg = img;

  return true;
}

// create DC handle if it is not initialized, or resume from hibernate
static void
winimg_lazyinit(imglist *img)
{
  BITMAPINFO bmpinfo;
  unsigned char *pixels;
  HDC dc;
  size_t size;
 
  if (img->hdc)
    return;

  size = img->pixelwidth * img->pixelheight * 4;

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
  if (img->pixels) {
    CopyMemory(pixels, img->pixels, size);
    free(img->pixels);
  } else {
    // resume from hibernation
    fflush((FILE *)img->fp);
    fseek((FILE *)img->fp, 0L, SEEK_SET);
    fread(pixels, 1, size, (FILE *)img->fp);
    fclose((FILE *)img->fp);
  }
  img->pixels = pixels;

  ReleaseDC(wnd, dc);
}

// serialize an image into a temp file to save the memory
static void
winimg_hibernate(imglist *img)
{
  FILE *fp;
  size_t nbytes;
  size_t size;

  size = img->pixelwidth * img->pixelheight * 4;

  if (!img->hdc)
    return;
  fp = tmpfile();
  if (!fp)
    return;
  nbytes = fwrite(img->pixels, 1, size, fp);
  if (nbytes != size) {
    fclose(fp);
    return;
  }

  // delete allocated DIB section.
  DeleteDC(img->hdc);
  DeleteObject(img->hbmp);
  img->pixels = NULL;
  img->hdc = NULL;
  img->hbmp = NULL;

  img->fp = fp;
}

void
winimg_destroy(imglist *img)
{
  if (img->hdc) {
    DeleteDC(img->hdc);
    DeleteObject(img->hbmp);
  } else if (img->pixels) {
    free(img->pixels);
  } else {
    fclose((FILE *)img->fp);
  }
  free(img);
}

void
winimgs_clear(void)
{
  imglist *img, *prev;

  // clear images in current screen
  for (img = term.imgs.first; img; ) {
    prev = img;
    img = img->next;
    winimg_destroy(prev);
  }

  // clear images in alternate screen
  for (img = term.imgs.altfirst; img; ) {
    prev = img;
    img = img->next;
    winimg_destroy(prev);
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
  HDC dc;
  RECT rc;

  dc = GetDC(wnd);

  GetClientRect(wnd, &rc);
  IntersectClipRect(dc, rc.left + PADDING, rc.top + PADDING,
                    rc.left + PADDING + term.cols * cell_width,
                    rc.top + PADDING + term.rows * cell_height);
  for (img = term.imgs.first; img;) {
    // if the image is out of scrollback, collect it
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
      // if the image is scrolled out, serialize it into a temp file.
      left = img->left;
      top = img->top - term.virtuallines - term.disptop;
      if (top + img->height < 0 || top > term.rows) {
        winimg_hibernate(img);
      } else {
        // create DC handle if it is not initialized, or resume from hibernate
        winimg_lazyinit(img);
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
        img->refresh = false;
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
