// Copyright (c) Wojciech Figat. All rights reserved.

#pragma once

#include "Engine/Core/Types/String.h"
#include "Engine/Core/ISerializable.h"
#include "Engine/Core/Collections/Dictionary.h"

API_ENUM() enum class AudioEffects
{
    None,
    Reverb,
    Echo,
    Delay,
    Chorus,
    Flanger,
    Distortion,
    Compressor,
    Limiter,
    Equalizer,
    PitchShifter,
    LowPass,
    HighPass
};

API_STRUCT() struct AudioBus : ISerializable
{
    API_AUTO_SERIALIZATION();
    DECLARE_SCRIPTING_TYPE_MINIMAL(AudioBus);

    /// <summary>
    ///  Name of Audio Bus
    /// </summary>
    API_FIELD(Attributes = "EditorOrder(10)")
    String Name;

    /// <summary>
    /// Volume in dB of Audio Bus
    /// </summary>
    API_FIELD(Attributes = "EditorOrder(20), Limit(-80, 20)")
    float VolumeDB = 0.0f;
    
    /// <summary>
    /// If Audio Bus is muted or not
    /// </summary>
    API_FIELD(Attributes = "EditorOrder(30)")
    bool IsMuted;

    /// <summary>
    /// Collects all the effects a bus might have
    /// </summary>
    API_FIELD(Attributes = "EditorOrder(40)")
    Dictionary<int,AudioEffects> BusEffects;

    /// <summary>
    /// Report which bus will be conditioned
    /// </summary>
    API_FIELD(Attributes = "EditorOrder(50)")
    String ParentBus;
};
