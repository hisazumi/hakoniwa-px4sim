#ifndef _DRONE_CONTROL_PROXY_HPP_
#define _DRONE_CONTROL_PROXY_HPP_

#include "utils/main_status.hpp"
#include "hako/pdu/hako_pdu_accessor.hpp"

#define GAME_CTRL_CHECK_COUNT_MAX       10
#define GAME_CTRL_BUTTON_RADIO_CONTROL  0
/*
#define GAME_CTRL_BUTTON_BAGGAGE_GRAB   1
#define GAME_CTRL_BUTTON_CAMERA_TAKE    2
#define GAME_CTRL_BUTTON_TAKEOFF_LAND   3
*/

#define GAME_CTRL_AXIS_UP_DOWN          1
#define GAME_CTRL_AXIS_LR_RR            0
#define GAME_CTRL_AXIS_LEFT_RIGHT       2
#define GAME_CTRL_AXIS_FORWARD_BACK     3

namespace hako::assets::drone {

class DroneControlProxy {
private:
    int button_check_count[4] = {};
    bool lastButtonState[4] = {};
    bool is_button_state_change(int index) {
        bool currentButtonState = (cmd_game.button[index] != 0);
        if (currentButtonState && !lastButtonState[index]) {
            this->button_check_count[index] = 1;
        } else if (!currentButtonState && lastButtonState[index]) {
            if (this->button_check_count[index] >= GAME_CTRL_CHECK_COUNT_MAX) {
                lastButtonState[index] = currentButtonState;
                return true;
            }
            this->button_check_count[index] = 0;
        } else if (currentButtonState) {
            if (this->button_check_count[index] < GAME_CTRL_CHECK_COUNT_MAX) {
                this->button_check_count[index]++;
            }
        }
        lastButtonState[index] = currentButtonState;
        return false;
    }
    bool radio_control_on = false;


    double home_pos_x = 0;
    double home_pos_y = 0;
    double home_pos_z = 0;
    MainStatus state;
    Hako_HakoDroneCmdTakeoff cmd_takeoff = {};
    Hako_HakoDroneCmdLand cmd_land = {};
    Hako_HakoDroneCmdMove cmd_move = {};
    Hako_GameControllerOperation cmd_game = {};

    hako::assets::drone::IAirCraft *drone;
    template<typename PacketType>
    bool read_cmd(int channel_id, PacketType& packet)
    {
        return do_io_read_cmd(drone, channel_id, packet);
    }
    template<typename PacketType>
    bool write_cmd(int channel_id, const PacketType& packet)
    {
        return do_io_write_cmd(drone, channel_id, packet);
    }

    void do_reply()
    {
        switch (state.get_status())
        {
            case MAIN_STATUS_TAKINGOFF:
                cmd_takeoff.header.request = false;
                cmd_takeoff.header.result = true;
                cmd_takeoff.header.result_code = 0;
                write_cmd(HAKO_AVATOR_CHANNEL_ID_CMD_TAKEOFF, cmd_takeoff);

                //for test
                //cmd_move.header.request = true;
                //cmd_move.x = 5.0;
                //cmd_move.y = 5.0;
                //cmd_move.speed = 5.0;
                //write_cmd(HAKO_AVATOR_CHANNEL_ID_CMD_MOVE, cmd_move);
                break;
            case MAIN_STATUS_LANDING:
                cmd_land.header.request = false;
                cmd_land.header.result = true;
                cmd_land.header.result_code = 0;
                write_cmd(HAKO_AVATOR_CHANNEL_ID_CMD_LAND, cmd_land);
                break;
            case MAIN_STATUS_MOVING:
                cmd_move.header.request = false;
                cmd_move.header.result = true;
                cmd_move.header.result_code = 0;
                write_cmd(HAKO_AVATOR_CHANNEL_ID_CMD_MOVE, cmd_move);
                break;
            default:
                break;
        }
    }


public:
    mi_drone_control_in_t in = {};
    void setup()
    {
        std::cout << "Setup pdu data" << std::endl;
        //for test
        //cmd_takeoff.header.request = true;
        //cmd_takeoff.height = 5.0;
        //cmd_takeoff.speed = 5.0;
        write_cmd(HAKO_AVATOR_CHANNEL_ID_CMD_TAKEOFF, cmd_takeoff);
        write_cmd(HAKO_AVATOR_CHANNEL_ID_CMD_MOVE, cmd_move);
        write_cmd(HAKO_AVATOR_CHANNEL_ID_CMD_LAND, cmd_land);
        write_cmd(HAKO_AVATOR_CHANNEL_ID_GAME_CTRL, cmd_game);
        this->radio_control_on = false;
    }

    DroneControlProxy(hako::assets::drone::IAirCraft *obj) 
    {
        this->drone = obj;
        home_pos_x = this->drone->get_drone_dynamics().get_pos().data.x;
        home_pos_y = this->drone->get_drone_dynamics().get_pos().data.y;
        home_pos_z = this->drone->get_drone_dynamics().get_pos().data.z;
    }
    virtual ~DroneControlProxy() {}

    bool need_control()
    {
        if (this->radio_control_on) {
            return true;
        }
        if (state.get_status() == MAIN_STATUS_LANDED) {
            return false;
        }
        return true;
    }
    void do_event(bool is_operation_doing)
    {
        if (read_cmd(HAKO_AVATOR_CHANNEL_ID_GAME_CTRL, cmd_game)) {
            if (this->is_button_state_change(GAME_CTRL_BUTTON_RADIO_CONTROL)) {
                this->radio_control_on = !this->radio_control_on;
                std::cout << "radio_control: " << this->radio_control_on << std::endl;
            }
            if (this->radio_control_on) {
                in.target.attitude.roll = cmd_game.axis[GAME_CTRL_AXIS_LEFT_RIGHT];
                in.target.attitude.pitch = cmd_game.axis[GAME_CTRL_AXIS_FORWARD_BACK];
                in.target.throttle.power = cmd_game.axis[GAME_CTRL_AXIS_UP_DOWN];
                in.target.direction_velocity.r = cmd_game.axis[GAME_CTRL_AXIS_LR_RR];
            }
        }
        in.target_stay = 0;
        if (state.get_status() == MAIN_STATUS_LANDED) {
            if (read_cmd(HAKO_AVATOR_CHANNEL_ID_CMD_TAKEOFF, cmd_takeoff) && cmd_takeoff.header.request) {
                state.takeoff();
                in.target_pos_z = -cmd_takeoff.height;
                in.target_pos_x = home_pos_x;
                in.target_pos_y = home_pos_y;
                in.target_velocity = cmd_takeoff.speed;
                std::cout << "takeoff: z = " << in.target_pos_z << std::endl;
                std::cout << "takeoff: x = " << in.target_pos_x << std::endl;
                std::cout << "takeoff: y = " << in.target_pos_y << std::endl;
            }
        }
        else if (state.get_status() == MAIN_STATUS_HOVERING) {
            if (read_cmd(HAKO_AVATOR_CHANNEL_ID_CMD_LAND, cmd_land) && cmd_land.header.request) {
                if (is_operation_doing) {
                    return;
                }
                state.land();
                in.target_pos_z = -cmd_land.height;
                in.target_pos_x = this->drone->get_drone_dynamics().get_pos().data.x;
                in.target_pos_y = this->drone->get_drone_dynamics().get_pos().data.y;
                in.target_velocity = cmd_land.speed;
                std::cout << "land: z = " << in.target_pos_z << std::endl;
                std::cout << "land: x = " << in.target_pos_x << std::endl;
                std::cout << "land: y = " << in.target_pos_y << std::endl;

            }
            else if (read_cmd(HAKO_AVATOR_CHANNEL_ID_CMD_MOVE, cmd_move) && cmd_move.header.request) {
                if (is_operation_doing) {
                    return;
                }
                std::cout << "START MOVE" << std::endl;
                state.move();
                in.target_pos_x = cmd_move.x;
                in.target_pos_y = cmd_move.y;
                in.target_pos_z = -cmd_move.z;
                std::cout << "move: z = " << in.target_pos_z << std::endl;
                std::cout << "move: x = " << in.target_pos_x << std::endl;
                std::cout << "move: y = " << in.target_pos_y << std::endl;
            }
            else {
                in.target_stay = 1;
            }
        }
        else {
            //TODO            
            in.target_stay = 1;
        }
    }
    void do_control(bool is_operation_doing)
    {
        switch (state.get_status())
        {
            case MAIN_STATUS_TAKINGOFF:
            case MAIN_STATUS_LANDING:
            case MAIN_STATUS_MOVING:
                if (!is_operation_doing) {
                    do_reply();
                    state.done();
                }
                break;
            case MAIN_STATUS_CANCELING:
                //TODO not supported
                break;
            case MAIN_STATUS_LANDED:
            case MAIN_STATUS_HOVERING:
            default:
                //nothing to do
                break;
        }
    }    

};

class DroneControlProxyManager {
private:
    AirCraftModuleSimulator module_simulator;
    std::vector<DroneControlProxy> drone_control_proxies;
public:
    void init(Hako_uint64 microseconds, Hako_uint64 dt_usec)
    {
        module_simulator.init(microseconds, dt_usec);
        for (auto& module : module_simulator.get_modules()) {
            DroneControlProxy proxy(module.drone);
            proxy.in.context = module.get_context();
            proxy.in.mass = module.drone->get_drone_dynamics().get_mass();
            proxy.in.drag = module.drone->get_drone_dynamics().get_drag();
            drone_control_proxies.push_back(proxy);
        }
    }
    void setup()
    {
        for (auto& proxy : drone_control_proxies) {
            proxy.setup();
        }
    }
    void do_task()
    {
        module_simulator.do_task();
    }

    void run()
    {
        int index = 0;
        for (auto& module : module_simulator.get_modules()) {
            DroneControlProxy& proxy = drone_control_proxies[index];
            hako::assets::drone::DroneDynamicsInputType drone_input = {};
            mi_drone_control_out_t out = {};
            DronePositionType pos = module.drone->get_drone_dynamics().get_pos();
            DroneEulerType angle = module.drone->get_drone_dynamics().get_angle();
            hako::assets::drone::DroneVelocityBodyFrameType velocity = module.drone->get_drone_dynamics().get_vel_body_frame();
            hako::assets::drone::DroneAngularVelocityBodyFrameType angular_velocity = module.drone->get_gyro().sensor_value();
            proxy.do_event(module.control_module.controller->is_operation_doing(module.get_context()));
            proxy.in.pos_x = pos.data.x;
            proxy.in.pos_y = pos.data.y;
            proxy.in.pos_z = pos.data.z;
            proxy.in.euler_x = angle.data.x;
            proxy.in.euler_y = angle.data.y;
            proxy.in.euler_z = angle.data.z;
            proxy.in.u = velocity.data.x;
            proxy.in.v = velocity.data.y;
            proxy.in.w = velocity.data.z;
            proxy.in.p = angular_velocity.data.x;
            proxy.in.q = angular_velocity.data.y;
            proxy.in.r = angular_velocity.data.z;

            if (proxy.need_control()) {
                out = module.control_module.controller->run(&proxy.in);
                proxy.do_control(module.control_module.controller->is_operation_doing(module.get_context()));
            }

            DroneThrustType thrust;
            DroneTorqueType torque;
            thrust.data = out.thrust;
            torque.data.x = out.torque_x;
            torque.data.y = out.torque_y;
            torque.data.z = out.torque_z;

#if 0
            auto mixer = module.drone->get_mixer();
            PwmDuty duty = mixer.run(thrust.data, torque.data.x, torque.data.y, torque.data.z);
            for (int i = 0; i < ROTOR_NUM; i++) {
                drone_input.controls[i] = duty.d[i];
                module.controls[i] = duty.d[i];
            }
            bool ret = mixer.testReconstruction(thrust.data, torque.data.x, torque.data.y, torque.data.z);
            if (ret == false) {
                std::cout << "ERROR: can not reconstruct.." << std::endl;
            }
#endif
            drone_input.no_use_actuator = true;
            drone_input.manual.control = false;
            drone_input.thrust = thrust;
            drone_input.torque = torque;
            if (module.drone->get_drone_dynamics().has_collision_detection()) {
                do_io_read_collision(module.drone, drone_input.collision);
            }
            if (module.drone->is_enabled_disturbance()) {
                do_io_read_disturb(module.drone, drone_input.disturbance);
            }
            module.drone->run(drone_input);
            calculate_simple_controls(module, thrust);
            do_io_write(module.drone, module.controls);
            index++;
        }
    }
};

}

#endif /* _DRONE_CONTROL_PROXY_HPP_ */