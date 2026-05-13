/*
  Q Light Controller Plus
  audiochannelconfig.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "audiochannelconfig.h"

AudioChannelConfig AudioChannelConfig::defaults()
{
    // In-class member initializers are the single source of truth for default
    // values — see EnvelopeConfig / NoiseGateConfig / TriggerConfig in
    // audiochannelconfig.h. This function intentionally adds no overrides.
    return AudioChannelConfig{};
}
