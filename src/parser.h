#ifndef PARSER_H_
#define PARSER_H_

#include "args.h"

class Parser {
  public:
    static void ParseArgs(int argc, char *argv[], Args &args);
    static void ParseDevice(Args &args);
    static void ParseWsUrl(Args &args);
};

#endif // PARSER_H_
