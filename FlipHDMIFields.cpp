#include "FlipHDMIFields.h"

#include "Logger.h"
#include "rx_lib.h"
#include "rx_cfg.h"
#include "rx_hal.h"
#include "Hapi_extra.h"
#include "ADV7842_cp_map_fct.h"

void FlipHDMIFields(void)
{

  /*
    FGR - we think 1080i fields are being recorded out of order; this
    allows us to flip the Field bit on the various interlaced video
    streams
  */

  LOG(Logger::NOTICE) << "HDMI: flipping field order." << std::flush;
  VRX_set_AV_INV_F(1);
}
