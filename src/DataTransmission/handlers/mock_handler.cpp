#include <algorithm>

#include "mock_handler.hpp"
#include "DataTypes/commands.hpp"
#include "logger/logger_api.hpp"

#define EXPECTED_MESSAGE_SIZE 4

MockHandler::MockHandler()
{
    create_commands();
}

MockHandler::~MockHandler()
{
    for (auto c : command_list)
    {
        delete c;
    }
}

void MockHandler::create_commands()
{
    const int number_of_nodes = 1;

    // for (auto y = 0; y < (2 * number_of_nodes) - 2; y++)
    // {
        //  command_list.push_back(new InitQubit(y));
         
    // }
    // command_list.push_back(new Nop());
    // command_list.push_back(new SingleQGate(0, Angle_t::_MINUS_PI, Axis_t::Z_AXIS));
    // bool do_h_gate = false;
    // bool measure_x_basis = false;
    // int nop_count = 12;
    // if (do_h_gate)
    // { 
    //     command_list.push_back(new SingleQGate(0, Angle_t::_MINUS_PI, Axis_t::Z_AXIS));
    //     command_list.push_back(new SingleQGate(0, Angle_t::_8_PI_16, Axis_t::Y_AXIS));
    // }
    // for (int i = 1; i < nop_count; i++)
    // {
    //     command_list.push_back(new Nop());
    // }
    // if (measure_x_basis)
    // {
    //     command_list.push_back(new SingleQGate(0, Angle_t::_MINUS_PI, Axis_t::Z_AXIS));
    //     command_list.push_back(new SingleQGate(0, Angle_t::_8_PI_16, Axis_t::Y_AXIS));
    // }
    // command_list.push_back(new SingleQGate(0, Angle_t::_MINUS_PI, Axis_t::Z_AXIS));
    // X Y X rotation sequence for pre-measurement gates
    // pi/2 pi/2 pi/2 is equivalent to hadamard gate
    // -pi/2 -pi/2 -pi is equivalent to Y basis measurement
    // command_list.push_back(new PreMeasureGateSelect(Angle_t::_8_PI_16, Angle_t::_8_PI_16, Angle_t::_8_PI_16, 0));
    // command_list.push_back(new QMeasure(0, 0, Axis_t::Z_AXIS));

    // Select a state and measurement basis for testing gate noise
    bool prep_one_state = false;
    bool prep_plus_state = false;
    bool prep_iplus_state = false;
    bool measure_x_basis = false;
    bool measure_y_basis = false;

    // Prepare one of the following states: |0>, |1>, |+>, or |i+>
    command_list.push_back(new InitQubit(0));
    if (prep_one_state)
    {
        command_list.push_back(new SingleQGate(0, Angle_t::_MINUS_PI, Axis_t::X_AXIS));
    }
    else if (prep_plus_state)
    {
        command_list.push_back(new SingleQGate(0, Angle_t::_MINUS_PI, Axis_t::Z_AXIS));
        command_list.push_back(new SingleQGate(0, Angle_t::_8_PI_16, Axis_t::Y_AXIS));
    }
    else if (prep_iplus_state)
    {
        command_list.push_back(new SingleQGate(0, Angle_t::_MINUS_8_PI_16, Axis_t::X_AXIS));
    }

    // Here we want to do an identity operation to test gate noise
    if (prep_plus_state)
    {
        // We do an X gate since this is equivalent to an identity operation on the |+> state
        command_list.push_back(new SingleQGate(0, Angle_t::_MINUS_PI, Axis_t::X_AXIS));
    }
    else if (prep_iplus_state)
    {
        // We do a Y gate since this is equivalent to an identity operation on the |i+> state
        command_list.push_back(new SingleQGate(0, Angle_t::_MINUS_PI, Axis_t::Y_AXIS));
    }
    else {
        // We do a Z gate since this is equivalent to an identity operation on the |0> state
        command_list.push_back(new SingleQGate(0, Angle_t::_MINUS_PI, Axis_t::Z_AXIS));
    }

    if (measure_x_basis)
    {
        command_list.push_back(new PreMeasureGateSelect(Angle_t::_8_PI_16, Angle_t::_8_PI_16, Angle_t::_8_PI_16, 0));
    }
    else if (measure_y_basis)
    {
        command_list.push_back(new PreMeasureGateSelect(Angle_t::_MINUS_8_PI_16, Angle_t::_MINUS_8_PI_16, Angle_t::_MINUS_PI, 0));
    }
    command_list.push_back(new QMeasure(0, 0, Axis_t::Z_AXIS));
}

void MockHandler::fill_mock_message(uint8_t const* output)
{

    if (!command_list.empty())
    {
        if (command_list[message_num]->conditional_execution(latest_measure_response))
        {
            command_list[message_num]->create_command((uint8_t*)output, 4);
        }

        message_num++;
        latest_measure_response = -1;

        if (message_num == command_list.size())
            message_num = 0;
    }
}

int8_t MockHandler::send_data(const uint8_t *data_to_send, uint8_t size)
{
    // no where to send at  the moment. so just log it
    if (size > 4)
        return -1;

    latest_measure_response = ((data_to_send[1] & 0x40) >> 6);
    return -1;
}

int8_t MockHandler::receive_data(uint8_t *received_data, uint8_t size)
{
    if (size > EXPECTED_MESSAGE_SIZE)
    {
        LOG_ERROR("MockHandler: receive_data called with size larger than expected");
        return -1;
    }
    fill_mock_message(received_data);

    return 0;
}

int8_t MockHandler::transfer(uint8_t const* tx_buffer, uint8_t const* rx_buffer, const uint8_t length)
{
    // if(length != EXPECTED_MESSAGE_SIZE)
    // {
    //     return -1;
    // }
    for (int i = 0; i < length/4; i++)
    {
        fill_mock_message(rx_buffer + i * EXPECTED_MESSAGE_SIZE);
    }

    return 0;
}