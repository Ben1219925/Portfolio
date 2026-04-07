import rclpy
from rclpy.node import Node

import numpy as np
from sensor_msgs.msg import LaserScan
from ackermann_msgs.msg import AckermannDriveStamped, AckermannDrive
from rclpy.qos import qos_profile_sensor_data
from nav_msgs.msg import Odometry
import math

class WallFollow(Node):
    def __init__(self):
        super().__init__('wall_follow_node')

        self.lidar_subscription_ = self.create_subscription(
            LaserScan,
            '/scan',
            self.scan_callback,
            qos_profile_sensor_data)
        
        self.odom_subscription_ = self.create_subscription(
			Odometry,
			'/odom',
			self.odom_callback,
			qos_profile_sensor_data)

        self.drive_publisher_ = self.create_publisher(AckermannDriveStamped, '/drive', 10)

        # PID parameters
        self.kp = 1.    
        self.ki = 0.   
        self.kd = 0.05  

        # PID state variables
        self.integral = 0.0
        self.prev_error = 0.0
        self.prev_time = None 
		
        self.vel_ = 0.

        self.reversing = False
        self.reversed = False
        self.reverse_angle = 0.

    def odom_callback(self, msg):
        # save the forward velocity of the turtlebot to vel
        self.vel_ = abs(msg.twist.twist.linear.x)

    def scan_callback(self, scan_msg):
        drive_msg = AckermannDriveStamped()
        
        # lidar fron 20 degrees
        angles = np.linspace(scan_msg.angle_min, scan_msg.angle_max, len(scan_msg.ranges))

        front_angle_range = np.deg2rad(20)  
        front_indices = np.where(np.abs(angles) < front_angle_range)

        front_ranges = np.array(scan_msg.ranges)[front_indices]
        dist = np.min(front_ranges)
       	ttc = 0.
        #ttc logic requires we are moving to avoid divide by zero. The 1.75 m is an extra check to ensure we don't get closer than approximately 1.75 m to an object
        if self.vel_ >= 0.01 and dist > 1.75:
            # -----WALL FOLLOWING-----
            # Left 45 degrees
            a = scan_msg.ranges[720]  
            # Left 90 degrees
            b = scan_msg.ranges[900] 
            theta = np.deg2rad(45)
            alpha = np.arctan((a*np.sin(theta) - b)/(a*np.cos(theta)))
            Dt = b*np.cos(alpha)
            L = 1
            Dt1 = Dt + L*np.sin(alpha)
            error = 0.75 - Dt1  
            
            # Update the derivative term: change in error over time
            current_time = self.get_clock().now().nanoseconds / 1e9
            if self.prev_time is None:
                delta_time = 1.0  
            else:
                delta_time = current_time - self.prev_time

            # Update the integral term: 
            self.integral += error * delta_time

            
            if delta_time > 0:        
                derivative = (error - self.prev_error) / delta_time
            else:
                derivative = 0

            # Save the current time and error for the next iteration
            self.prev_time = current_time
            self.prev_error = error

            # PID control for steering
            pid_output = self.kp * error + self.ki * self.integral + self.kd * derivative

            # Steering angle based on PID output
            if abs(pid_output) >= 0.1:
                drive_msg.drive.steering_angle = -pid_output  
            else:
                drive_msg.drive.steering_angle = 0.0 

            # -----AUTOMATIC EMERGENCY BREAKING-----
            # Using TTC=d/v we want to stop 2 seconds before collision
            ttc = dist/self.vel_
            if ttc <= 1:
                #self.get_logger().info("Stopping ttc")
                self.stop_robot(scan_msg, drive_msg.drive.steering_angle)
                return
        else:
            # WALL FOLLOWING
            # If the robot is too close to the wall, adjust steering based on which side is closer
            if scan_msg.ranges[720] <= scan_msg.ranges[360]:
                # turn fully right
                drive_msg.drive.steering_angle = -1.0 
            else:
                # turn fully left
                drive_msg.drive.steering_angle = 1.0 
            #AEB
            #if we aren't moving at least 0.1 m/s check to see if there is an object within 0.5 m if there is handle stop condition
            if dist >= 1.5:
                self.reversing = False
                self.reversed = False
            if dist <= 0.75 or self.reversing:
                self.reversing = True
                self.stop_robot(scan_msg, drive_msg.drive.steering_angle)
                return
            
        # Speed control based on the proximity of the wall
        angle = drive_msg.drive.steering_angle
        if 0 <= abs(angle) < np.deg2rad(10):
            drive_msg.drive.speed = 4.0
        elif np.deg2rad(10) <= abs(angle) < np.deg2rad(20):
            drive_msg.drive.speed = 2.0
        else:
            drive_msg.drive.speed = 1.0
            
        # Publish the drive message
        self.drive_publisher_.publish(drive_msg)

    def stop_robot(self, scan_msg, angle):
        # If too close to the wall, reverse slowly and reverse the wheel direction
        drive_msg = AckermannDriveStamped()
        if not self.reversed:
            self.reversed = True
            self.reverse_angle = -angle
        drive_msg.drive.steering_angle = self.reverse_angle
        drive_msg.drive.speed = -1.
        # Publish the drive message
        self.drive_publisher_.publish(drive_msg)


def main(args=None):
    rclpy.init(args=args)
    print("WallFollow Initialized")
    wall_follow_node = WallFollow()
    rclpy.spin(wall_follow_node)

    wall_follow_node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()

