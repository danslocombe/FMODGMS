#pragma once

#include <vector>

namespace AudioProcessors
{
    struct ASDRConfig
    {
        double Attack;
        double Sustain;
        double Decay;
        double Release;

        double ValDown(double input) const
        {
            if (input < Attack)
            {
                return input / Attack;
            }
            else if (input < Attack + Decay)
            {
                const double slope = (Sustain - 1.0) / Decay;
                return slope * (input - Attack) + 1.0;
            }
            
            return Sustain;
        }

        double ValUp(double input)
        {
            if (input < Release)
            {
                return Sustain * (Release - input) / Release;
            }

            return 0.0;
        }
    };

    float Compress(float x, float mult, float thresh, float ramp);
    std::vector<float> HighPass(const std::vector<float>& data, float alpha);

    class LowPass
    {
        float m_finalValue;
    public:
        std::vector<float> Run(const std::vector<float>& data, float alpha)
        {
            const size_t len = data.size();
            std::vector<float> ret;
            ret.resize(len);

            float prev = m_finalValue;

            for (uint32_t i = 0; i < len; i++)
            {
                const float x = prev + alpha * (data[i] - prev);
                prev = x;
                ret[i] = x;
            }

            m_finalValue = (ret[len - 1]);

            return ret;
        }
    };

    inline double lerp(double x0, double x1, double k)
    {
        return (x0 * (k - 1) + x1) / k;
    }
}
