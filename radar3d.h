#ifndef _RADAR3D_H
#define _RADAR3D_H

#define MAX_SHAPE_PTS 48

// GTA V styled tilted radar, LimitRadarPoint clamping is done by main.cpp
namespace Radar3D
{
    bool Install(void* hGTASA, const bool* displayingMap);
    bool IsActive();

    // Radar space <-> tilted radar space
    void Forward(float& u, float& v);
    void Inverse(float& u, float& v);

    // Settings
    void SetEnabled(bool enable);
    void SetOnFootTilt(float strength);    // 0.0 - 0.7
    void SetVehicleTilt(float strength);   // 0.0 - 0.7
    void SetFrontShift(float shift);       // 0.0 - 0.6, always front radar
    void SetForwardShift(float shift);     // -0.6 - 0.6
    void SetZoom(float zoom);              // 1.0 - 3.0
    void SetCompensateZoom(bool enable);
    void SetFlatBlips(bool enable);
    void SetBlipDepthScale(float scale);   // 0.0 - 1.0
    void SetExtendedStreaming(bool enable);
    void SetHideOffRadarBlips(bool enable);
    void SetDebug(bool enable);
    void SetTiltSpeed(float scale);

    void NoteBlipClamped(bool outside);
    void SetShapeBoundary(const float* uvPairs, int count); // Less than 3 points is a circle
}

#endif // _RADAR3D_H
