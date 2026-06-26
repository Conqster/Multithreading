#include "Core.h"
#include <thread>

#include "TaskCoordinator.h"


#define NOMINMAX
#include "Window.h"

#include "libs/tracy/tracy/Tracy.hpp"
//#define TRACY_ENABLE //inside globall, propertites C/C++ preprocessor Definitions


struct ProgramContext
{
	std::atomic<int> i = 0;
};


struct TimeTaken
{
	void Start()
	{
		mStart = Clock::now();
	}

	using Clock = std::chrono::steady_clock;
	//using ClockDuration = std::chrono::duration;
	std::chrono::time_point<Clock> mStart;


	float Duration()
	{
		auto end = Clock::now();
		float ms = std::chrono::duration<float, std::milli>(end - mStart).count();
		return ms;
	}
};

namespace Rand{
	static inline float Float(float min = 0.0, float max = 1.0)
	{
		return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
	}
}


struct Vec2
{
	Vec2() = default;
	explicit Vec2(float v) : x(v), y(v){ }
	Vec2(float _x, float _y) : 
		x(_x), y(_y){ }


	Vec2 operator+(const Vec2 v) const { return { x + v.x, y + v.y }; }
	Vec2& operator+=(const Vec2 v)
	{
		x += v.x;
		y += v.y;
		return *this;
	}


	Vec2 operator-(const Vec2 v) const { return { x - v.x, y - v.y }; }
	Vec2& operator-=(const Vec2 v)
	{
		x -= v.x;
		y -= v.y;
		return *this;
	}


	Vec2 operator*(const Vec2 v) const { return { x * v.x, y * v.y };}
	Vec2& operator*=(const Vec2 v)
	{
		x *= v.x;
		y *= v.y;
		return *this;
	}
	Vec2 operator*(float v) const { return { x * v, y * v };}
	Vec2& operator*=(float v)
	{
		x /= v;
		y /= v;
		return *this;
	}

	//componnent wise 
	Vec2 operator/(const Vec2 v) const { return { x / v.x, y / v.y };}
	Vec2& operator/=(const Vec2 v)
	{
		x /= v.x;
		y /= v.y;
		return *this;
	}
	
	Vec2 operator/(float v) const { return { x / v, y / v };}

	Vec2 Abs() const { return { std::abs(x), std::abs(y) }; }
	bool IsNan() const { return std::isnan(x) || std::isnan(y); }


	float Dot(const Vec2& v) const { return (x * v.x) + (y * v.y); }
	float Length() const { return std::sqrt((x * x) + (y * y)); }

	Vec2 Normalised() const
	{
		return *this / Length();
	}

	static Vec2 Clamp(const Vec2& v, const Vec2& _min, const Vec2& _max)
	{
		return { std::min(std::max(v.x, _min.x), _max.x),
		std::min(std::max(v.y, _min.y), _max.y) };
	}

	friend std::ostream& operator<<(std::ostream& ios, const Vec2& v)
	{
		ios << "{x: " << v.x << ", y: " << v.y << "}";
		return ios;
	}

	float x, y;
};

struct Particle
{
	Vec2 position{0.0f};
	Vec2 velocity{0.0f};
	uint32_t cellIdx = 0xffffffff;
};


struct GridCell
{
	//static const Vec2 size = Vec2(100, 50);
};

int ParticleCellLocation(const Particle& p)
{
	PROFILE_SCOPE;
	Vec2 grid_half_size = Vec2(100, 50) * 2;

	Vec2 ratio = p.position / grid_half_size;

	int x;
	if (ratio.y < 0.0f)
		x = (ratio.y < -0.5f) ? 3 : 2;
	else 
		x = (ratio.y > 0.5f) ? 0 : 1;

	int y;
	if (ratio.x > 0.0f)
		y = (ratio.x > 0.5f) ? 3 : 2;
	else
		y = (ratio.x < -0.5f) ? 0 : 1;


	return (4 * x) + y;
}


void CollideWithBounds(Particle& p)
{
	PROFILE_SCOPE;
	Vec2 abs_pos = p.position.Abs();

	Vec2 dir = Vec2(1.0f);
	
	float contact_impluse = 50.0f;
	Vec2 impluse_dir{ 0.0f };
	//if (abs_pos.x > 100 * 2)
	//{
	//	dir.x = -1;
	//	p.position.x = std::signbit(p.position.x) * 98 * 2;
	//	impluse_dir.x = std::signbit(p.position.x) * -1.0f;
	//}
	////if (abs_pos.y > 50 * 2)

	if (p.position.x > 100 * 2)
	{
		dir.x = -1;
		p.position.x = 98 * 2;
		impluse_dir.x = -1.0f;
	}
	else if (p.position.x < (-100 * 2))
	{
		dir.x = -1;
		p.position.x = (-98 * 2);
		impluse_dir.x = 1.0f;
	}

	if (p.position.y > 50 * 2)
	{
		dir.y = -1;
		p.position.y = 48 * 2;
		impluse_dir.y = -1.0f;
	}
	else if (p.position.y < (-50 * 2))
	{
		dir.y = -1;
		p.position.y = (-48 * 2);
		impluse_dir.y = 1.0f;
	}
		
	p.velocity *= dir;

	//impluse
	p.velocity += impluse_dir * contact_impluse;
}


void CollideWithBounds(Particle* particles, uint32_t count)
{
	PROFILE_SCOPE;
	ASSERT(count > 1);

	///Bruteforce N squared 
	for (Particle* pA = particles, *pA_end = particles + count;
		pA < pA_end; ++pA)
	{
		CollideWithBounds(*pA);
	}
}



static void CollideParticleParticle(Particle* particles, uint32_t count)
{
	PROFILE_SCOPE;
	ASSERT(count > 1);

	//TimeTaken _time;
	//_time.Start();

	float arbitrary_radius = 0.5f;
	///Bruteforce N squared 
	for (Particle* pA = particles, *pA_end = particles + count;
		pA < pA_end; ++pA)
	{
		for (Particle* pB = particles+1, *pB_end = particles + count;
			pB < pB_end; ++pB)
		{
			if (pA == pB) continue;

			Vec2 disp = (pB)->position - (pA)->position;

			if (std::abs(disp.Dot(disp)) > arbitrary_radius * arbitrary_radius)
				continue;

			//Vec2 nor = disp.Normalised();
			//float mag = disp.Length();


			//resolve 
			(pA)->velocity -= disp;
			(pB)->velocity += disp;

		}
	}



	//float duration_ms = _time.Duration();
	//LOG_MSG("CollideParticleParticle duration: " << duration_ms << "ms - " << duration_ms * 0.001 << "s\n");
}

void SimulateGravity(Particle& p, const Vec2& gravity_force, float dt)
{
	PROFILE_SCOPE;
	Vec2 acc = gravity_force;

	Vec2 vel = acc *dt;

	p.velocity += vel;


	ASSERT(!p.position.IsNan());
	ASSERT(!p.velocity.IsNan());
	//p.velocity *= max(0.0f, 1.0f - 0.8f * dt);
	//p.velocity *= max(0.0f, 0.7f * dt);
	p.velocity *= 0.7f;
	ASSERT(!p.position.IsNan());
	ASSERT(!p.velocity.IsNan());


	//clamp vel 
	p.velocity = Vec2::Clamp(p.velocity, Vec2(-100, -150), Vec2(100, 150));

	p.position += p.velocity * dt;
}

void SimulateGravity(const Vec2& gravity_force, float dt, Particle* particles, uint32_t count)
{
	PROFILE_SCOPE;
	ASSERT(count > 1);

	Vec2 acc = gravity_force;

	Vec2 vel = acc * dt;

	for (Particle* p = particles, *p_end = particles + count;
		p < p_end; ++p)
	{
		p->velocity += vel;

		ASSERT(!p->position.IsNan());
		ASSERT(!p->velocity.IsNan());
		//p.velocity *= max(0.0f, 1.0f - 0.8f * dt);
		//p.velocity *= max(0.0f, 0.7f * dt);
		p->velocity *= 0.7f;
		ASSERT(!p->position.IsNan());
		ASSERT(!p->velocity.IsNan());


		//clamp vel 
		p->velocity = Vec2::Clamp(p->velocity, Vec2(-100, -150), Vec2(100, 150));

		p->position += p->velocity * dt;
	}
}


#include <iomanip>

void DrawASCII(const Particle& p)
{
	PROFILE_SCOPE;
	int particle_cell_count = 0;

	for (int r = 0; r < 4; ++r)
	{
		LOG_MSG("+---------+---------+---------+---------+\n|");
		for (int c = 0; c < 4; ++c)
		{
			int cell_idx = r * 4 + c;

			if (cell_idx == ParticleCellLocation(p))
				particle_cell_count = 1;
			else
				particle_cell_count = 0;
			//LOG_MSG(" " << std::setw(7) << 100 << " |");
			LOG_MSG(" " << std::setw(7) << particle_cell_count << " |");
		}
		LOG_MSG("\n|");
		for (int c = 0; c < 4; ++c)
		{
			int cell_idx = r * 4 + c;
			LOG_MSG(" " << std::setw(6 - ((cell_idx > 9) ? 1 : 0)) << "Cell_" << cell_idx << " |");
		}
		LOG_MSG("\n");
	}
	LOG_MSG("+---------+---------+---------+---------+\n");
}

#include <array>

void DrawASCII(const std::vector<Particle>& particles)
{
	PROFILE_SCOPE;
	///idx0
	///idx1
	/// idx2 
	/// 
	/// etc 
	/// 
	std::array<int, 16> particles_cell;
	particles_cell.fill(0);
	for (const auto& p : particles)
	{
		int idx = ParticleCellLocation(p);
		particles_cell[idx]++;
	}

	for (int r = 0; r < 4; ++r)
	{
		LOG_MSG("+---------+---------+---------+---------+\n|");
		for (int c = 0; c < 4; ++c)
		{
			int cell_idx = r * 4 + c;
			int particle_cell_count = particles_cell[cell_idx];
			LOG_MSG(" " << std::setw(7) << particle_cell_count << " |");
		}
		LOG_MSG("\n|");
		for (int c = 0; c < 4; ++c)
		{
			int cell_idx = r * 4 + c;
			LOG_MSG(" " << std::setw(6 - ((cell_idx > 9) ? 1 : 0)) << "Cell_" << cell_idx << " |");
		}
		LOG_MSG("\n");
	}
	LOG_MSG("+---------+---------+---------+---------+\n");
}

void DrawASCII(const std::vector<Particle>* particles)
{
	///idx0
	///idx1
	/// idx2 
	/// 
	/// etc 
	/// 
	std::array<int, 16> particles_cell;
	for (int i = 0; i < 16; ++i)
	{
		particles_cell[i] = particles[i].size();
	}

	for (int r = 0; r < 4; ++r)
	{
		LOG_MSG("+---------+---------+---------+---------+\n|");
		for (int c = 0; c < 4; ++c)
		{
			int cell_idx = r * 4 + c;
			int particle_cell_count = particles_cell[cell_idx];
			LOG_MSG(" " << std::setw(7) << particle_cell_count << " |");
		}
		LOG_MSG("\n|");
		for (int c = 0; c < 4; ++c)
		{
			int cell_idx = r * 4 + c;
			LOG_MSG(" " << std::setw(6 - ((cell_idx > 9) ? 1 : 0)) << "Cell_" << cell_idx << " |");
		}
		LOG_MSG("\n");
	}
	LOG_MSG("+---------+---------+---------+---------+\n");
}

#define MULTITHREADED_TASKCOORD 1
int main()
{
	TimeTaken time_taken;
	LOG_MSG("Hello " << "Multicore for physics\n");

	int num_threads = 0;
	num_threads = std::thread::hardware_concurrency() - 1;

	ProgramContext ctx;
#if MULTITHREADED_TASKCOORD
	TaskCoordinatorThreads* task_coord = new TaskCoordinatorThreads(num_threads);
#else
	TaskCoordinatorSequential* task_coord = new TaskCoordinatorSequential;
#endif // MULTITHREADED_TASKCOORD

	time_taken.Start();
	std::atomic<int> atom_i = 1;
	for (int i = 1; i < 500; ++i)
	{
		task_coord->ConstructTask([&]()
	//	//job_coord.EnqueueJob([&ctx]()
		{
				//need to prevent race conditon
				int c = atom_i++;
				
				volatile double v = 0.0;
				for (int step = 0; step < 500000; ++step)
					v += std::sin(double(step)) * std::cos(double(step));

				int sq = c * c;
				//LOG_MSG("Complete "<< i << ", v: " << v << " c_sq:" << sq << "\n");
			}, 0);

	}
	task_coord->WaitForTasks();

	//job_coord.EndThreads();
	
	float duration_ms = time_taken.Duration();
	LOG_MSG("Excution duration: " << duration_ms << "ms - " << duration_ms * 0.001 << "s\n");
	//job_coord.EndThreads();

	Particle p;
	p.position = Vec2(0, 100);
	p.velocity = Vec2(Rand::Float(-50, 50));
	p.velocity = Vec2(4, 0);

	std::vector<Particle> partciles;
	//std::vector<Particle> cell_partciles[16 * 5] = {};
	std::vector<Particle> cell_partciles[128] = {};

	for (auto& cell : cell_partciles)
		cell.reserve(1000);

	partciles.reserve(10000);
	for (int i = 0; i < 10000; ++i)
	{
		Particle p;
		p.position = Vec2(Rand::Float(-200, 200), Rand::Float(-100, 100));
		p.velocity = Vec2(Rand::Float(-10, 10));
		partciles.push_back(p);
	}

	DrawASCII(p);
	//std::cout << "\033H" << std::flush;
	//system("pause");
	system("cls");
	DrawASCII(p);

	glfwInit();

	bool sim_single = false;

	double mLastFrameTime = glfwGetTime();
	float time_scale = 0.5f;

	re_run_task_loop:
	system("cls");
	std::vector<float> list_duration;
	list_duration.reserve(100);

	if (task_coord == nullptr)
#if MULTITHREADED_TASKCOORD
		task_coord = new TaskCoordinatorThreads;
#else
		task_coord = new TaskCoordinatorSequential;
#endif // MULTITHREADED_TASKCOORD
	//tasks 
	Task* add_extra_cell_detail_task = nullptr;
	Task* add_extra_cell_detail_task2 = nullptr;
	Task* build_solve_particle_task = nullptr;
	std::vector<Task*> solve_particle_tasks;
	//job_coord.BeginThreads(num_threads);
	for (int i = 0; i < 100; ++i)
	{
		FrameMark;
		double curr_frame_time = glfwGetTime();
		double mFrameDeltaTime = curr_frame_time - mLastFrameTime;
		mLastFrameTime = curr_frame_time;

		mFrameDeltaTime *= time_scale;

		Vec2 gravity{ 0.0f, -9.81f };
		float gravity_scale = 0.01f;
		gravity *= gravity_scale;

		if(sim_single)
		{
			time_taken.Start();
			CollideParticleParticle(partciles.data(), partciles.size());
			//SimulateGravity(p, gravity, mFrameDeltaTime);
			//CollideWithBounds(p);

			float duration_ms = time_taken.Duration();
			LOG_MSG("Excution duration: " << duration_ms << "ms - " << duration_ms * 0.001 << "s\n");
			system("cls");
			DrawASCII(p);

			LOG_MSG("Particle position: " << p.position << "\n");
			LOG_MSG("Particle velocity: " << p.velocity << "\n");
		}
		else
		{


			//for (auto& p : partciles)
			//{
			//	p.cellIdx = ParticleCellLocation(p);
			//	cell_partciles[p.cellIdx].push_back(p);
			//}
			//for (auto& p : partciles)
			//{
			//	p.cellIdx = ParticleCellLocation(p) + 16;
			//	cell_partciles[p.cellIdx].push_back(p);
			//}
			//CollideParticleParticle(partciles.data(), partciles.size());

			//for (auto& cell : cell_partciles)
			//{
			//	if(cell.size() > 1)
			//	{
			//		CollideParticleParticle(cell.data(), cell.size());
			//		SimulateGravity(gravity, mFrameDeltaTime, cell.data(), cell.size());
			//		CollideWithBounds(cell.data(), cell.size());
			//	}
			//}


			//tasks 
			add_extra_cell_detail_task = nullptr;
			add_extra_cell_detail_task2 = nullptr;
			build_solve_particle_task = nullptr;
			solve_particle_tasks.clear();

			build_solve_particle_task = task_coord->ConstructTask([&cell_partciles, &solve_particle_tasks, &task_coord, &gravity, &mFrameDeltaTime, &time_taken]()
				{
					time_taken.Start();
					//int i = 0;
					//for (int i = 0; i < cell_partciles->size(); ++i)
					for (auto& cell : cell_partciles)
					{
						//std::vector<Particle>& cell = cell_partciles[i];
						if (cell.size() > 1)
						{
							//solve_particle_tasks[i++] = task_coord->ConstructTask([&raw_data, &cell_size, &gravity, &mFrameDeltaTime, i]() //i to track
							solve_particle_tasks.push_back(task_coord->ConstructTask([raw_data = cell.data(), cell_size = cell.size(), &gravity, &mFrameDeltaTime]() //i to track
								{
									///later have dependacies a collide with bounds depends 
									///on simulate gravity as gravity simulation could cause over shoot
									CollideParticleParticle(raw_data, cell_size);
									SimulateGravity(gravity, mFrameDeltaTime, raw_data, cell_size);
									CollideWithBounds(raw_data, cell_size);
								}, 1)
							); ///wait build_solve_particle_task
						}
					}

					//for (auto& task : solve_particle_tasks)
					//	task->RemoveDependency();
					task_coord->RemoveTasksDependency(solve_particle_tasks.data(), solve_particle_tasks.size());

				}, 2); ///depends on add_extra_cell_detail_task & add_extra_cell_detail_task2


			add_extra_cell_detail_task = task_coord->ConstructTask([&partciles, &cell_partciles, build_solve_particle_task]() ///&build_solve_particle_task pass by ref for now 
				{


					for (auto& p : partciles)
					{
						p.cellIdx = ParticleCellLocation(p) + 32;
						cell_partciles[p.cellIdx].push_back(p);
					}

					for (auto& p : partciles)
					{
						p.cellIdx = ParticleCellLocation(p) + 48;
						cell_partciles[p.cellIdx].push_back(p);
					}

					for (auto& p : partciles)
					{
						p.cellIdx = ParticleCellLocation(p) + 64;
						cell_partciles[p.cellIdx].push_back(p);
					}
					
					build_solve_particle_task->RemoveDependency();
				}, 1); //depends on clear task


			add_extra_cell_detail_task2 = task_coord->ConstructTask([&partciles, &cell_partciles, build_solve_particle_task]() ///&build_solve_particle_task pass by ref for now 
				{
					for (auto& p : partciles)
					{
						p.cellIdx = ParticleCellLocation(p) + 80;
						cell_partciles[p.cellIdx].push_back(p);
					}

					for (auto& p : partciles)
					{
						p.cellIdx = ParticleCellLocation(p) + 96;
						cell_partciles[p.cellIdx].push_back(p);
					}

					for (auto& p : partciles)
					{
						p.cellIdx = ParticleCellLocation(p) + 112;
						cell_partciles[p.cellIdx].push_back(p);
					}

					build_solve_particle_task->RemoveDependency();
				}, 1); //depends on clear task

			//can now kivk off tasks
			//clear_cell_task->RemoveDependency();
			Task* clear_cell_task = task_coord->ConstructTask([&cell_partciles, &partciles, add_extra_cell_detail_task, add_extra_cell_detail_task2]()
				{
					for (auto& cell : cell_partciles)
						cell.clear();

					for (auto& p : partciles)
					{
						p.cellIdx = ParticleCellLocation(p);
						cell_partciles[p.cellIdx].push_back(p);
					}

					for (auto& p : partciles)
					{
						p.cellIdx = ParticleCellLocation(p) + 16;
						cell_partciles[p.cellIdx].push_back(p);
					}

					add_extra_cell_detail_task->RemoveDependency();
					add_extra_cell_detail_task2->RemoveDependency();
				}, 0); //kick off instantly


			task_coord->WaitForTasks();

			
			float duration_ms = time_taken.Duration();
			system("cls");
			//DrawASCII(partciles);
			//DrawASCII(cell_partciles);
			LOG_MSG("Excution duration: " << duration_ms << "ms - " << duration_ms * 0.001 << "s\n");
			list_duration.push_back(duration_ms);
		}
		//DrawASCII(partciles);

			

		//std::this_thread::sleep_for(std::chrono::microseconds(60));
		std::this_thread::yield();
	}
#if MULTITHREADED_TASKCOORD
	task_coord->QuitAndEndThreads();
#endif // MULTITHREADED_TASKCOORD


	float average_duration = 0.0f;
	//bool on_side = true;
	int on_side = 0;
	for(const auto& duration : list_duration)
	{
		char buff[8];
		std::snprintf(buff, sizeof(buff), "%6.2f", duration);// , ((duration - duration) * 100));
		//std::snprintf(buff, sizeof(buff), "%02f%01f", duration);// , ((duration - duration) * 100));
		//if(on_side)
		if((on_side%3)==0)
			LOG_MSG("duration: " << buff << "ms.\n");
		else
			LOG_MSG("duration: " << buff << "ms. " << std::setw(2) << "| ");

		average_duration += float(duration);
		//on_side ^= 1;
		on_side++;
	}
	average_duration /= list_duration.size();
	LOG_MSG("duration average: " << average_duration << "ms.\n");

	for (auto& cell : cell_partciles)
		cell.clear();

	///re_run 100 times
	static int re_run_count = 0;
#ifdef _DEBUG
	if (re_run_count < 35)
#else
	if (re_run_count < 50)
#endif // _DEBUG
	{
		re_run_count++;
		std::this_thread::yield();
		delete task_coord;
		task_coord = nullptr;
		goto re_run_task_loop;
	}

	char _t;
	LOG_MSG("Re-run: [y/n]?");
	std::cin >> _t;
	if (_t == 'y')
	{
		re_run_count = 0;
		delete task_coord;
		task_coord = nullptr;
		goto re_run_task_loop;
	}

	return 0;
}
