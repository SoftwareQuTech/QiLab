/*
** Created on: 22.07.2026
** Created by: Claude
** Description: Contains the entanglement-channel noise model for the
**              simulator manager (QILab_Noise_Models.pdf Sec. 4.3). One
**              instance per link (pair of nodes).
**
**              Noise channel: dephasing (p_z) composed with depolarizing
**              (p_d), collapsed into a single biased single-qubit Pauli
**              channel (Eq. 17) applied to one qubit of the pair.
**
**              Out-of-range parameters are rejected, not repaired: the model
**              is marked invalid (is_valid() == false), an error is logged,
**              and the channel falls back to the identity.
*/

#pragma once
#include <string>

#include "Simulator/entanglement_channel_noise_interface.hpp"
#include "Simulator/pauli_channel_math.hpp"
#include "logger/logger_api.hpp"

struct EntanglementChannelBasicConfig
{
    float p_depolarizing;
    float p_dephasing;
};

class EntanglementChannelModel : public EntanglementChannelNoiseInterface
{
public:
    EntanglementChannelModel(float p_depolarizing, float p_dephasing)
    {
        if (p_depolarizing < 0.0f || p_depolarizing > 1.0f)
        {
            LOG_ERROR("Invalid entanglement depolarizing probability " + std::to_string(p_depolarizing) +
                      ". Must be in [0, 1]. Model marked invalid.");
            valid = false;
        }
        if (p_dephasing < 0.0f || p_dephasing > 1.0f)
        {
            LOG_ERROR("Invalid entanglement dephasing probability " + std::to_string(p_dephasing) +
                      ". Must be in [0, 1]. Model marked invalid.");
            valid = false;
        }

        if (!valid)
        {
            return; // Weights stay at the identity channel
        }

        // Combined dephasing-then-depolarizing channel as one Pauli channel (Eq. 17):
        // a pure-Z dephasing channel composed with a symmetric depolarizing channel.
        weights = compose_pauli_channels(0.0f, 0.0f, p_dephasing,
                                         p_depolarizing / 3.0f, p_depolarizing / 3.0f, p_depolarizing / 3.0f);
    }

    ~EntanglementChannelModel() override = default;

    bool is_valid() const
    {
        return valid;
    }

    PauliChannelWeights get_pauli_channel_weights() override
    {
        return weights;
    }

private:
    PauliChannelWeights weights{1.0f, 0.0f, 0.0f, 0.0f}; // Identity channel until validated
    bool valid = true;
};
