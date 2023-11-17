#ifndef annotate_h
#define annotate_h

#include <string>

namespace lux
{
  using namespace std;
  
  class Annotate
  {
  public:
    static std::vector<std::string> s_logs;

    static void circle(int x, int y)
    {
      s_logs.push_back("dc " + to_string(x) + " " + to_string(y));
    }

    static void x(int x, int y)
    {
      s_logs.push_back("dx " + to_string(x) + " " + to_string(y));
    }

    static void line(int x1, int y1, int x2, int y2)
    {
      s_logs.push_back("dl " + to_string(x1) + " " + to_string(y1) + " " +  to_string(x2) + " " + to_string(y2));
    }

    static void text(int x1, int y1, const string& message)
    {
      s_logs.push_back("dt " + to_string(x1) + " " + to_string(y1) + " '" +  message + "' 16");
    }

    static void text(int x1, int y1, const string& message, int fontsize)
    {
      s_logs.push_back("dt " + to_string(x1) + " " + to_string(y1) + " '" +  message + "' " + to_string(fontsize));
    }

    static void sidetext(const string &message)
    {
      s_logs.push_back("dst '" + message + "'");
    }
  };
}


#endif
