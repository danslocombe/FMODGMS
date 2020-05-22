#pragma once

namespace Cassette
{
    class CassetteControl
    {
    public:
        CassetteControl(double sampleLength);

        void StartPlaying();
        void StopPlaying();

        void SetPos(double pos);
        void SetVel(double vel);

        // Delta time measured in samples
        void Tick(double len);

        double GetPos() const;
        double GetVel() const;

    private:
        double m_sampleLength;
        double m_pos;
        // Measured in samples / second
        double m_vel;

        bool m_playing;

        double GetTargetVel() const;
    };
}