#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>

#include <native/task.h>
#include <native/timer.h>

///// TCP
#include "Poco/Net/Net.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Exception.h"
#include "Poco/Timer.h"
#include "Poco/Stopwatch.h"
#include "Poco/Thread.h"
#include "Poco/DateTime.h"
#include "Poco/Timespan.h"
#include "Poco/NumericString.h"
#include <iostream>
#include <time.h>

/// ROS
#include "ros/ros.h"
#include "std_msgs/String.h"
#include "geometry_msgs/Pose.h"

#include <sstream>

using namespace Poco;
using Poco::Net::SocketAddress;
using Poco::Net::StreamSocket;
//using Poco::Net::Socket;
//using Poco::Timer;
//using Poco::TimerCallback;
//using Poco::Thread;
//using Poco::Stopwatch;
using namespace std;


RT_TASK demo_task;

// HYU-SPA
RT_TASK tcp_task;
// 
RT_TASK roslstn_task;
RT_TASK rostlkr_task;
// VIVE
RT_TASK rosvive1_task;
RT_TASK rosvive2_task;

std::stringstream listener;

//const std::string hostname = "127.0.0.1"; //localhost IP Address
const std::string hostname = "192.168.0.101"; //STEP2 IP Address 
//const std::string hostname = "192.168.0.100"; //STEP2 IP Address Monkey
//const std::string hostname = "192.168.1.18"; //STEP2 IP Address Tensegrity
//const std::string hostname = "192.168.0.122"; //STEP2 IP Address Tensegrity
const Poco::UInt16 PORT = 9911;

/* NOTE: error handling omitted. */

enum {
	SIZE_HEADER = 52,
	SIZE_COMMAND = 4,
	SIZE_HEADER_COMMAND = 56,
	SIZE_DATA_MAX = 200,
	SIZE_DATA_ASCII_MAX = 32
};

union Data
{
	unsigned char byte[SIZE_DATA_MAX];
	float float6dArr[9];
};




void demo(void *arg)
{
	RTIME now, previous;

	/*
	 * Arguments: &task (NULL=self),
	 *            start time,
	 *            period (here: 1 s)
	 */
	rt_task_set_periodic(NULL, TM_NOW, 1e6);
	previous = rt_timer_read();

	while (1) {
		
		now = rt_timer_read();

		/*
		 * NOTE: printf may have unexpected impact on the timing of
		 *       your program. It is used here in the critical loop
		 *       only for demonstration purposes.
		 */
		//printf("Time since last turn: %ld.%06ld ms\n",
		//       (long)(now - previous) / 1000000,
		//       (long)(now - previous) % 1000000);
		       previous = now;
		rt_task_wait_period(NULL);
		}
}


void tcp_run(void *arg)
{
	/*
	 * Arguments: &task (NULL=self),
	 *            start time,
	 *            period (here: 1 s)
	 */
	rt_task_set_periodic(NULL, TM_NOW, 1e6);
	

	StreamSocket ss;
	int cnt = 0;
	int flag_return = 0;
	int rpt_cnt = 1;
	clock_t start, check, end, init;
	int motion_buf = 0;
	double glob_time, loop_time;
	Data data_rev, data;
	unsigned char readBuff[1024];
	unsigned char writeBuff[1024];
	
	try
	{
		cout << "Trying to connect server..." << endl;
		ss.connect(SocketAddress(hostname, PORT));

		Timespan timeout(1, 0);
		while (ss.poll(timeout, Poco::Net::Socket::SELECT_WRITE) == false)
		{
			cout << "Connecting to server..." << endl;
		}
		//cout << "Complete to connect server" << endl;

		//cout << "=========== Please enter the packet ============" << endl;
		//cout << "Packet scheme\n| 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |" << endl;
		//cout << "0 : Motion number, 1~6 : Desired data, 7 : Null" << endl;
		//cout << "================================================" << endl;

		while (true)
		{
			//cout << rpt_cnt << " : ";
			/*try {
				cin >> data.float6dArr[0] >> data.float6dArr[1] >> data.float6dArr[2] >> data.float6dArr[3] >> data.float6dArr[4] >>
					data.float6dArr[5] >> data.float6dArr[6] >> data.float6dArr[7];
				memcpy(writeBuff, data.byte, SIZE_DATA_MAX);
				if ((int)data.float6dArr[0] < 100)
				{
					throw (int)data.float6dArr;
				}
				ss.sendBytes(writeBuff, 1024);
			}
			catch (int expn) {
				cout << "[ERROR] : Please check the Motion" << endl;
				return 0;
			}
			if (data.float6dArr[6] < 1.5 && (int)data.float6dArr[0]==106)
				cout << "[WARNING] : Please enter a trajectory time longer than 1.5s" << endl;
			if ((int)data.float6dArr[0] == 101 && motion_buf == 101)
			{
				cout << "[WARNING] : Homming has already done" << endl;
			}
			motion_buf = (int)data.float6dArr[0];
			*/
			data.float6dArr[0]=rpt_cnt;
			memcpy(writeBuff, data.byte, SIZE_DATA_MAX);

			ss.sendBytes(writeBuff, 1024);
			while (ss.poll(timeout, Poco::Net::Socket::SELECT_READ) == false)
			{
				if (cnt >= 5) {
					flag_return = 1;
					break;
				}
				cout << "Waiting to receive data..." << endl;
				cnt++;
			}
			if (flag_return == 0)
			{
				ss.receiveBytes(readBuff, 1024);
				memcpy(data_rev.byte, readBuff, SIZE_DATA_MAX);
				//cout<<(int)data_rev.float6dArr[0]<<endl;

			}
			else
			{
				cout << "No response from server..." << endl;
				break;
			}
			rpt_cnt++;
			rt_task_wait_period(NULL);
		}
		ss.close();
	}
	catch (Poco::Exception& exc)
	{
		cout << "Fail to connect server..." << exc.displayText() << endl;
	}
}

void chatterCallback(const std_msgs::String::ConstPtr& msg)
{
  ROS_INFO("Data Received: [%s]", msg->data.c_str());
  //listener.str() = msg->data;
}

void roslstn_run(void *arg)
{
	/*
	 * Arguments: &task (NULL=self),
	 *            start time,
	 *            period (here: 1 ms)
	 */
	RTIME now, previous;
	previous = rt_timer_read();

	rt_task_set_periodic(NULL, TM_NOW, 5e7);
	
	int ros_cnt = 0;


	ros::NodeHandle n;

	ros::Subscriber sub_lstn = n.subscribe("/chatter2", 1000, chatterCallback);

	while (true)
	{
		now = rt_timer_read();

	/*	printf("Time since last turn: %ld.%06ld ms\n",
		       (long)(now - previous) / 1000000,
		       (long)(now - previous) % 1000000);*/
	       previous = now;		

		//cout<<listener.str()<<endl;
		ros::spinOnce();
		rt_task_wait_period(NULL);
	}

}

void rostlkr_run(void *arg)
{
	/*
	 * Arguments: &task (NULL=self),
	 *            start time,
	 *            period (here: 1 ms)
	 */
	rt_task_set_periodic(NULL, TM_NOW, 5e7);
	
	int ros_cnt = 0;


	//ros::init(argc, argv, "listener2");
	ros::NodeHandle n;

	ros::Publisher chatter_pub = n.advertise<std_msgs::String>("/talker2", 1000);

	while (true)
	{
	  	std_msgs::String msg;

		std::stringstream ss;
		ss << "hello world " << ros_cnt;
   		msg.data = ss.str();
		
   		//ROS_INFO("%s", msg.data.c_str());

		chatter_pub.publish(msg);
		ros_cnt++;
		rt_task_wait_period(NULL);
	}

}


void vive_tracker1_Callback(const geometry_msgs::Pose::ConstPtr& msg)
{
  ROS_INFO("Position{x:%f, y:%f, z:%f}, Orientation:{x:%f, y:%f, z:%f, w:%f}", 
	msg->position.x, msg->position.y, msg->position.z, 
	msg->orientation.x, msg->orientation.y, msg->orientation.z, msg->orientation.w
	);
}

void rosvive1_run(void *arg)
{
	/*
	 * Arguments: &task (NULL=self),
	 *            start time,
	 *            period (here: 1 ms)
	 */
	RTIME now, previous;
	previous = rt_timer_read();

	rt_task_set_periodic(NULL, TM_NOW, 5e7);
	
	int ros_cnt = 0;


	ros::NodeHandle n;

	ros::Subscriber sub_vive1 = n.subscribe("/vive1", 1000, vive_tracker1_Callback);

	while (true)
	{
		now = rt_timer_read();

	/*	printf("Time since last turn: %ld.%06ld ms\n",
		       (long)(now - previous) / 1000000,
		       (long)(now - previous) % 1000000);*/
	       previous = now;		

		//cout<<listener.str()<<endl;
		ros::spinOnce();
		rt_task_wait_period(NULL);
	}

}


void vive_tracker2_Callback(const geometry_msgs::Pose::ConstPtr& msg)
{
  ROS_INFO("Position x:%f, y:%f, z:%f, Orientation x:%f, y:%f, z:%f, w:%f", 
	msg->position.x, msg->position.y, msg->position.z, 
	msg->orientation.x, msg->orientation.y, msg->orientation.z, msg->orientation.w
	);
}

void rosvive2_run(void *arg)
{
	/*
	 * Arguments: &task (NULL=self),
	 *            start time,
	 *            period (here: 1 ms)
	 */
	RTIME now, previous;
	previous = rt_timer_read();

	rt_task_set_periodic(NULL, TM_NOW, 5e7);
	
	int ros_cnt = 0;


	ros::NodeHandle n;

	ros::Subscriber sub_vive2 = n.subscribe("/vive2", 1000, chatterCallback);

	while (true)
	{
		now = rt_timer_read();
	
		printf("VIVE2 has activated\n");
		//printf("Time since last turn: %ld.%06ld ms\n",
		//       (long)(now - previous) / 1000000,
		//       (long)(now - previous) % 1000000);
	       	//previous = now;		

		//cout<<listener.str()<<endl;
		ros::spinOnce();
		rt_task_wait_period(NULL);
	}

}

void catch_signal(int sig)
{
	rt_task_delete(&demo_task);
	rt_task_delete(&tcp_task);
	rt_task_delete(&roslstn_task);
	rt_task_delete(&rostlkr_task);
	rt_task_delete(&rosvive1_task);
	rt_task_delete(&rosvive2_task);
	printf("\nAll Tasks are stopped\n");

	exit(1);
}



int main(int argc, char* argv[])
{
	signal(SIGTERM, catch_signal);
	signal(SIGINT, catch_signal);

	/* Avoids memory swapping for this program */
	mlockall(MCL_CURRENT|MCL_FUTURE);
	// ros init
	ros::init(argc, argv, "MTP");
	/*
	 * Arguments: &task,
	 *            name,
	 *            stack size (0=default),
	 *            priority,
	 *            mode (FPU, start suspended, ...)
	 */
	// HYU-SPA	
	rt_task_create(&demo_task, "trivial", 0, 99, 0);
	rt_task_create(&tcp_task, "tcp", 0, 95, 0);
	
	rt_task_create(&roslstn_task, "roslstn", 0, 90, 0);
	rt_task_create(&rostlkr_task, "rostlkr", 0, 89, 0);
	// VIVE
	rt_task_create(&rosvive1_task, "rosvive1", 0, 88, 0);
	rt_task_create(&rosvive2_task, "rosvive2", 0, 87, 0);
	/*
	 * Arguments: &task,
	 *            task function,
	 *            function argument
	 */
	// HYU-SPA
	rt_task_start(&demo_task, &demo, NULL);
	rt_task_start(&tcp_task, &tcp_run, NULL);

	rt_task_start(&roslstn_task, &roslstn_run, NULL);
	rt_task_start(&rostlkr_task, &rostlkr_run, NULL);
	// VIVE
	rt_task_start(&rosvive1_task, &rosvive1_run, NULL);
	rt_task_start(&rosvive2_task, &rosvive2_run, NULL);
	pause();

	rt_task_delete(&demo_task);

	return 0;
}
