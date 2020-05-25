#include "CassetteControl.h"
#include <cmath>
#include "ConstantReader.h"

using namespace Cassette;

CassetteControl::CassetteControl(double sampleLength)
{
    m_pos = 0.0;
    m_vel = 0.0;
    m_playing = false;
    m_sampleLength = sampleLength;
}

void CassetteControl::StartPlaying()
{
    m_playing = true;
}

void CassetteControl::StopPlaying()
{
    m_playing = false;
}

void CassetteControl::SetPos(double pos)
{
    m_pos = pos;
}

void CassetteControl::SetVel(double vel)
{
    m_vel = vel;
}

double CassetteControl::GetPos() const
{
    return m_pos;
}

double CassetteControl::GetVel() const
{
    return m_vel;
}

// When playing aim toward 1 sample / sample
constexpr double TARGET_VEL = 1.0;

double CassetteControl::GetTargetVel() const
{
    if (m_playing)
    {
        return TARGET_VEL;
    }

    return 0.0;
}

double weightVals(double x0, double x1, double weight)
{
    return (x0 * (weight - 1.0) + x1) / weight;
}

void CassetteControl::Tick(double dt)
{
    if (dt == 0)
    {
        return;
    }

    const double targetVel = GetTargetVel();

    double weight_div = Constants::Globals.GetDouble("cassette_control_weight_divisor");

    if (weight_div == 0.0)
    {
        weight_div = 1.0;
    }

    double weight_base = 1.0 / weight_div;

    //const double WEIGHT = 1.0 / 200.0;
    //const double DECEL_WEIGHT = WEIGHT * 1.8;
    const double decel_weight = weight_base * Constants::Globals.GetDouble("cassette_control_weight_decel_mult");

    const double weight = std::abs(targetVel) > 0.001 ? weight_base : decel_weight;

    m_vel = weightVals(m_vel, targetVel, dt * weight);

    m_pos += m_vel * dt;
    if (m_pos > m_sampleLength)
    {
        m_pos -= m_sampleLength;
    }
}
