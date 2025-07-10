// Copyright (c) Wojciech Figat. All rights reserved.

#pragma once

#include "Engine/Core/Types/String.h"
#include "Engine/Core/ISerializable.h"
#include "Engine/Core/Collections/Array.h"


API_STRUCT() struct BusCompoment : ISerializable
{
    API_AUTO_SERIALIZATION();
    DECLARE_SCRIPTING_TYPE_MINIMAL(BusCompoment);

    /// <summary>
    ///  Name of Audio Bus
    /// </summary>
    API_FIELD(Attributes = "EditorOrder(10)")
    String Name;

    /// <summary>
    /// Volume of Audio Bus (which will be converted to decibel scale)
    /// </summary>
    API_FIELD(Attributes = "EditorOrder(20), Limit(0, 1)")
    float Volume = 1;
    
    /// <summary>
    /// If Audio Bus is muted or not
    /// </summary>
    API_FIELD(Attributes = "EditorOrder(30)")
    bool IsMuted;

    /// <summary>
    /// Collects all the effects a bus might have
    /// </summary>
    API_FIELD(Attributes = "EditorOrder(40)")
    Array<int> BusEffects;
};
