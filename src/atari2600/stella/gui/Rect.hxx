#ifndef RECT_HXX
#define RECT_HXX

// #include <cassert>
#include "bspf.hxx"

namespace GUI {

namespace Internal {

  inline bool parseInt(const char*& p, int& value)
  {
    if(!p || !*p)
      return false;

    bool negative = false;
    if(*p == '-')
    {
      negative = true;
      ++p;
    }

    if(*p < '0' || *p > '9')
      return false;

    int result = 0;
    while(*p >= '0' && *p <= '9')
    {
      result = result * 10 + (*p - '0');
      ++p;
    }

    value = negative ? -result : result;
    return true;
  }

  inline bool parsePair(const string& str, int& a, int& b)
  {
    const char* p = str.c_str();

    if(!parseInt(p, a))
      return false;

    if(*p != 'x')
      return false;
    ++p;

    if(!parseInt(p, b))
      return false;

    return *p == '\0';
  }

  inline string intToString(int value)
  {
    char buf[16];
    char tmp[16];
    int pos = 0;
    bool neg = false;

    if(value == 0)
      return "0";

    if(value < 0)
    {
      neg = true;
      value = -value;
    }

    while(value > 0)
    {
      tmp[pos++] = char('0' + (value % 10));
      value /= 10;
    }

    int out = 0;
    if(neg)
      buf[out++] = '-';

    while(pos > 0)
      buf[out++] = tmp[--pos];

    buf[out] = '\0';
    return string(buf);
  }

} // namespace Internal

struct Point
{
  int x;
  int y;

  Point() : x(0), y(0) {}
  Point(const Point& p) : x(p.x), y(p.y) {}
  explicit Point(int x1, int y1) : x(x1), y(y1) {}
  Point(const string& p)
  {
    if(!Internal::parsePair(p, x, y))
      x = y = 0;
  }

  Point& operator=(const Point& p) { x = p.x; y = p.y; return *this; }
  bool operator==(const Point& p) const { return x == p.x && y == p.y; }
  bool operator!=(const Point& p) const { return x != p.x || y != p.y; }

  string toString() const
  {
    return Internal::intToString(x) + "x" + Internal::intToString(y);
  }
};

struct Size
{
  int w;
  int h;

  Size() : w(0), h(0) {}
  Size(const Size& s) : w(s.w), h(s.h) {}
  explicit Size(int w1, int h1) : w(w1), h(h1) {}
  Size(const string& s)
  {
    if(!Internal::parsePair(s, w, h))
      w = h = -1;
  }

  Size& operator=(const Size& s) { w = s.w; h = s.h; return *this; }
  bool operator==(const Size& s) const { return w == s.w && h == s.h; }
  bool operator!=(const Size& s) const { return w != s.w || h != s.h; }

  string toString() const
  {
    return Internal::intToString(w) + "x" + Internal::intToString(h);
  }
};

struct Rect
{
  int top, left;
  int bottom, right;

  Rect() : top(0), left(0), bottom(0), right(0) {}
  Rect(int w, int h) : top(0), left(0), bottom(h), right(w) {}
  Rect(int x1, int y1, int x2, int y2) : top(y1), left(x1), bottom(y2), right(x2)
  {
    assert(isValidRect());
  }

  int x() const { return left; }
  int y() const { return top; }
  Point point() const { return Point(x(), y()); }

  int width() const  { return right - left; }
  int height() const { return bottom - top; }
  Size size() const  { return Size(width(), height()); }

  void setWidth(int aWidth) { right = left + aWidth; }
  void setHeight(int aHeight) { bottom = top + aHeight; }
  void setSize(const Size& size) { setWidth(size.w); setHeight(size.h); }

  bool contains(int x, int y) const {
    return (left <= x) && (x < right) && (top <= y) && (y < bottom);
  }

  bool contains(const Point& p) const { return contains(p.x, p.y); }

  bool intersects(const Rect& r) const {
    return (left < r.right) && (r.left < right) && (top < r.bottom) && (r.top < bottom);
  }

  void extend(const Rect& r) {
    left   = MIN(left, r.left);
    right  = MAX(right, r.right);
    top    = MIN(top, r.top);
    bottom = MAX(bottom, r.bottom);
  }

  void grow(int offset) {
    top -= offset;
    left -= offset;
    bottom += offset;
    right += offset;
  }

  void clip(const Rect& r) {
    assert(isValidRect());
    assert(r.isValidRect());

    if(top < r.top) top = r.top;
    else if(top > r.bottom) top = r.bottom;

    if(left < r.left) left = r.left;
    else if(left > r.right) left = r.right;

    if(bottom > r.bottom) bottom = r.bottom;
    else if(bottom < r.top) bottom = r.top;

    if(right > r.right) right = r.right;
    else if(right < r.left) right = r.left;
  }

  void clip(int maxw, int maxh) {
    clip(Rect(0, 0, maxw, maxh));
  }

  bool isValidRect() const {
    return (left <= right && top <= bottom);
  }

  void moveTo(int x, int y) {
    bottom += y - top;
    right += x - left;
    top = y;
    left = x;
  }

  void moveTo(const Point& p) {
    moveTo(p.x, p.y);
  }

  string toString() const
  {
    return string("Point: ") + point().toString() +
           ", Size: " + size().toString();
  }
};

} // namespace GUI

#endif