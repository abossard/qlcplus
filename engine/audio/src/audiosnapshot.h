/*
  Q Light Controller Plus
  audiosnapshot.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#pragma once

#include "aubioresults.h"

struct PerceptualBands
{
    double sub = 0.0;
    double bass = 0.0;
    double lowMid = 0.0;
    double mid = 0.0;
    double high = 0.0;
    double low = 0.0;
};

struct TriggerState
{
    double value = 0.0;
    bool active = false;
    bool firedThisFrame = false;
    bool releasedThisFrame = false;
    double heldMs = 0.0;
    double cooldownRemainingMs = 0.0;
};

/**
 * v3 audio snapshot, sourced from AubioProcessor + AudioChannel envelopes.
 * No more legacy 32-bin FFT spectrum or AGC fields.
 */
struct AudioSnapshot
{
    // Mel spectrum (40 bands from aubio filterbank)
    double mel[AUBIO_MEL_BANDS] = {};

    // MFCC (13 coefficients from aubio)
    double mfcc[AUBIO_MFCC_COEFFS] = {};

    // Perceptual bands derived from mel band grouping
    PerceptualBands bands;

    // Triggers (5 perceptual bands)
    TriggerState triggers[5];
    TriggerState volumeTrigger;
    TriggerState beatTrigger;

    struct
    {
        double raw = 0.0;
        double smoothed = 0.0;
        double normalized = 0.0;
    } volume;

    struct
    {
        bool beat = false;
        double bpm = 0.0;
        double beatPhase = 0.0;
        double beatConfidence = 0.0;
        bool tatum = false;
    } music;

    struct
    {
        double rmsDb = -96.0;
        double peakDb = -96.0;
        double crestFactor = 1.0;
        double centroidHz = 0.0;
        double spread = 0.0;
        double rolloffHz = 0.0;
        double flux = 0.0;
        double hfc = 0.0;
    } features;

    struct
    {
        bool energy = false;
        bool hfc = false;
        bool complex_ = false;
        bool phase = false;
        bool wphase = false;
        bool specdiff = false;
        bool kl = false;
        bool mkl = false;
        bool specflux = false;
        int voteCount = 0;
    } onsets;

    struct
    {
        double hz = 0.0;
        double confidence = 0.0;
    } pitch;

    struct
    {
        double midi = 0.0;
        double velocity = 0.0;
        bool noteOn = false;
        bool noteOff = false;
    } note;

    struct
    {
        double transientEnergy = 0.0;
        double steadyEnergy = 0.0;
        double ratio = 0.0;  // 0=all steady, 1=all transient
    } tss;

    double audioDtMs = 0.0;
    double brightnessFloor = 0.0;
    bool noiseGateClosed = false;
};
