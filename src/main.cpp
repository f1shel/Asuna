#include "tracer/tracer.h"

#include <ext/json.hpp>
#include <nvh/inputparser.h>

void help()
{
  LOGI("usage: asuna.exe [--help] [--out <path>] [--offline] [--scene <path>]\n\n");
  LOGI("   --scene   : input scene json\n");
  LOGI("   --out     : output file\n");
  LOGI("   --offline : disable gui and run offline\n");
}

int main(int argc, char** argv)
{
  // setup some basic things for the sample, logging file for example
  NVPSystem system(PROJECT_NAME);

  InputParser parser(argc, argv);
  if(parser.exist("--help"))
  {
    help();
    exit(0);
  }

  TracerInitSettings tis;
  if(parser.exist("--offline"))
    tis.offline = true;
  tis.outputname = parser.getString("--out", "asuna_out.hdr");
  tis.scenefile  = parser.getString("--scene", "scenes/dragon/scene.json");
  tis.sceneSpp   = parser.getInt("--spp");

  Tracer asuna;
  asuna.init(tis);
  asuna.run();
  asuna.deinit();
  return 0;
}