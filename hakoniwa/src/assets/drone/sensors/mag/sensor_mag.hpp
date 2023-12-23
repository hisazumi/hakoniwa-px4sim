#ifndef _SENSOR_MAG_HPP_
#define _SENSOR_MAG_HPP_

#include "isensor_mag.hpp"
#include "utils/icsv_log.hpp"
#include "../../utils/sensor_data_assembler.hpp"
#include "utils/csv_logger.hpp"

namespace hako::assets::drone {

class SensorMag : public hako::assets::drone::ISensorMag, public ICsvLog {
private:
    double delta_time_sec;
    double total_time_sec;
    hako::assets::drone::SensorDataAssembler mag_x;
    hako::assets::drone::SensorDataAssembler mag_y;
    hako::assets::drone::SensorDataAssembler mag_z;

public:
    SensorMag(double dt, int sample_num) : delta_time_sec(dt), mag_x(sample_num), mag_y(sample_num), mag_z(sample_num) 
    {
        this->noise = nullptr;
    }
    virtual ~SensorMag() {}
    void run(const DroneAngleType& angle) override
    {
        double theta = angle.data.y + params_I;
        double psi = angle.data.z - params_D;
        //std::cout << "theta: " << theta << " psi: " << psi << std::endl;

        double x =   params_F * (cos(theta) * cos(psi));
        double y = - params_F * (cos(theta) * sin(psi));
        double z = - params_F * sin(theta);

        this->mag_x.add_data(x);
        this->mag_y.add_data(y);
        this->mag_z.add_data(z);
        total_time_sec += delta_time_sec;
    }
    DroneMagDataType sensor_value() override
    {
        DroneMagDataType value;
        value.data.x = this->mag_x.get_calculated_value();
        value.data.y = this->mag_y.get_calculated_value();
        value.data.z = this->mag_z.get_calculated_value();
        if (this->noise != nullptr) {
            value.data.x = this->noise->add_noise(value.data.x);
            value.data.y = this->noise->add_noise(value.data.y);
            value.data.z = this->noise->add_noise(value.data.z);
        }
        return value;
    }
    void print() override
    {
        auto result = sensor_value();
        std::cout << "mag( "
                    << result.data.x
                    << ", "
                    << result.data.y
                    << ", "
                    << result.data.z
                    << " )" 
                    << std::endl;
    }
    const std::vector<std::string> log_head() override
    {
        return { "TIME", "X", "Y", "Z" };
    }
    const std::vector<std::string> log_data() override
    {
        auto v = sensor_value();

        return {std::to_string(CsvLogger::get_time_usec()), std::to_string(v.data.x), std::to_string(v.data.y), std::to_string(v.data.z)};
    }

};

}


#endif /* _SENSOR_MAG_HPP_ */