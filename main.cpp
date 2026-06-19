#include "Core.h"
#include <thread>

#include "TaskCoordinator.h"


#define NOMINMAX
#include "Window.h"



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
	TaskCoordinatorThreads* task_coord = new TaskCoordinatorThreads;
	task_coord->Init(num_threads);
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

	std::vector<float> list_duration;
	list_duration.reserve(100);
	//job_coord.BeginThreads(num_threads);
	for (int i = 0; i < 100; ++i)
	{
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
			Task* add_extra_cell_detail_task = nullptr;
			Task* add_extra_cell_detail_task2 = nullptr;
			Task* build_solve_particle_task = nullptr;
			std::vector<Task*> solve_particle_tasks;

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




			//for (int i = 0; i < cell_partciles->size(); ++i)
			//{
			//	std::vector<Particle>& cell = cell_partciles[i];
			//	if (cell.size() > 1)
			//	{
			//		Particle* raw_data = cell.data();
			//		uint32_t cell_size = cell.size();

			//		solve_particle_tasks[i] = task_coord->ConstructTask([&raw_data, &cell_size, &gravity, &mFrameDeltaTime]()
			//			{
			//				///later have dependacies a collide with bounds depends 
			//				///on simulate gravity as gravity simulation could cause over shoot
			//				CollideParticleParticle(raw_data, cell_size);
			//				SimulateGravity(gravity, mFrameDeltaTime, raw_data, cell_size);
			//				CollideWithBounds(raw_data, cell_size);
			//			}, 1); ///wait for add_extra_cell_detail_task
			//	}
			//}


		

			//for (auto& cell : cell_partciles)
			//{
			//	if (cell.size() > 1)
			//	{
			//		Particle* raw_data = cell.data();
			//		uint32_t cell_size = cell.size();
			//		task_coord->ConstructTask([&raw_data, &cell_size, &gravity, &mFrameDeltaTime]()
			//			{
			//					///later have dependacies a collide with bounds depends 
			//					///on simulate gravity as gravity simulation could cause over shoot
			//					CollideParticleParticle(raw_data, cell_size);
			//					SimulateGravity(gravity, mFrameDeltaTime, raw_data, cell_size);
			//					CollideWithBounds(raw_data, cell_size);
			//			}, 0);
			//	}
			//}
			//for (auto& cell : cell_partciles)
			//{
			//	if (cell.size() > 1)
			//	{
			//		Particle* raw_data = cell.data();
			//		uint32_t cell_size = cell.size();
			//		task_coord->ConstructTask([raw_data, cell_size, gravity, mFrameDeltaTime]()
			//			{
			//				///later have dependacies a collide with bounds depends 
			//				///on simulate gravity as gravity simulation could cause over shoot
			//				CollideParticleParticle(raw_data, cell_size);
			//				SimulateGravity(gravity, mFrameDeltaTime, raw_data, cell_size);
			//				CollideWithBounds(raw_data, cell_size);
			//			}, 0);
			//	}
			//}
			//for (auto& cell : cell_partciles)
			//{
			//	if (cell.size() > 1)
			//	{
			//		Particle* raw_data = cell.data();
			//		uint32_t cell_size = cell.size();
			//		task_coord->ConstructTask([raw_data, cell_size, gravity, mFrameDeltaTime]()
			//			{
			//				///later have dependacies a collide with bounds depends 
			//				///on simulate gravity as gravity simulation could cause over shoot
			//				CollideParticleParticle(raw_data, cell_size);
			//				SimulateGravity(gravity, mFrameDeltaTime, raw_data, cell_size);
			//				CollideWithBounds(raw_data, cell_size);
			//			}, 0);
			//	}
			//}

				////can kick of particle solving when detail are completed
				//i = 0;
				//for (auto& cell : cell_partciles)
				//	if (cell.size() > 1)
				////		solve_particle_tasks[i++]->RemoveDependency(); //this would cause N queue mutex lock as its added one after the other
				//for (auto& task : solve_particle_tasks)
				//	task->RemoveDependency();



			task_coord->WaitForTasks();

			
			float duration_ms = time_taken.Duration();
			system("cls");
			//DrawASCII(partciles);
			DrawASCII(cell_partciles);
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
	for(const auto& duration : list_duration)
	{
		LOG_MSG("duration: " << duration << "ms.\n");
		average_duration += float(duration);
	}
	average_duration /= list_duration.size();
	LOG_MSG("duration average: " << average_duration << "ms.\n");

	for (auto& cell : cell_partciles)
		cell.clear();

	for (auto& p : partciles)
	{
		p.cellIdx = ParticleCellLocation(p);
		cell_partciles[p.cellIdx].push_back(p);
	}
	uint32_t i = 0;
	for (auto& cell : cell_partciles)
		LOG_MSG("Cell " << i++ << ": " << cell.size() << "\n");
	//bool close_program = false;

	//Window window;
	//window.Init(900, 900, true);
	//bool mouse_cursor_lock = false;
	//window.LockCursor(mouse_cursor_lock);

	//auto Keyboard_input = [&](int key, int scancode, int action, int mods)
	//	{
	//		if (key == GLFW_KEY_Q && action == GLFW_PRESS)
	//		{
	//			window.LockCursor(mouse_cursor_lock = !mouse_cursor_lock);
	//		}

	//	};
	//window.SetKeyboardCallback(Keyboard_input);

	//while (window.IsActive())
	//{
	//	glClearColor(0, 0, 1, 1);
	//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	//	window.FlushAndSwapBuffer();
	//}


	return 0;
}



struct VertexPrimitive
{
	static const uint32_t kInvalidId = 0xffffffff;
	uint32_t vao = kInvalidId;
	uint32_t vbo = kInvalidId;
	uint32_t ibo = kInvalidId;
};



//
//
//void Worker::operator()()
//{
//	m_JobQueue.m_LogMutex.lock();
//	std::cout << "____    Thread " << id << " started. ____" << std::endl;
//	m_JobQueue.m_LogMutex.unlock();
//
//
//
//	/* done (&bool)
//	* done is a bool ref to the main program done,
//	This allows all the worker thread to perform execution & sleep (while no work/waiting for work).
//	* These pervent the destrustion of threads and creation of new ones every frame (expensive !!!!!!)
//	* So the worker keeps updating and waiting for jobs --> via jobFlag.
//	* When a job is found it locks the work queue lock mutex and trys to acquire a job from the queue, if available
//	* Then after the job is acquire (i.e data acquistion) the workQueue is available to other threads (its lock mutex is unlock i.e released)
//	*
//	* Then promptly acquires the workqueue Loglock mutex to excute job and print to the console and released.
//	* Which afterward ---> doneFlag.notify_one (notifies one waiting thread not all)
//	*/
//	while (!m_SysActive)
//	{
//		//wait to .2 of a sec for a signal. 
//		//std::unique_lock<std::mutex> queue_mutex(m_JobPool.m_PoolMutex);
//		std::unique_lock<std::mutex> queue_mutex(m_JobQueue.m_QueueMutex);
//
//
//		/*jobFlag --> (wait until)
//		Here I think we are using the job flag and also a unique lock with the work queue lock.
//		So any thread with the same work queue lock would have the same signal.
//		And the wait_until condition is to wait for a workQueue Not Done. and duration of wait is .2 of a second.
//
//
//		As i have read, the .wait, wait_for, wait_until, where current thread and all other thread instance acquire
//		then atomically release the mutex lock and suspend thread excution if for or until for set duration(timeout),
//		or condition notification*/
//		/* condition_variable  .wait
//		is used to wait/hault a current thread until the condition is met. the current must hold/share mutex with */
//
//		m_JobQueue.m_JobFlag.wait(queue_mutex, [this] {return !m_JobQueue.IsEmpty() ||
//			m_SysActive; });
//
//
//		//Exit if system is shutting down
//		if (m_SysActive) break;
//
//		Job* pending_job = m_JobQueue.Dequeue();
//		queue_mutex.unlock();
//
//		if (pending_job)
//		{
//			pending_job->Execute();
//			delete pending_job;
//		}
//		m_JobQueue.m_NotEmptyFlag.notify_one();
//		//m_JobPool.m_LogMutex.lock();
//		////std::cout << "____    Thread " << id << " Waiting for Jobs. _____" << std::endl;
//		////m_JobPool.m_LogMutex.unlock();
//
//		//if (access)
//		//{
//		//	Job* pending_job = m_JobQueue.NextJob();
//
//		//	//pop get the first job and pop it from the queue.
//		//	if (!pending_job) // <--- the job might have been empty 
//		//		continue;
//
//		//	//release lock (a unique ref to the work queue lock) 
//		//	//m_JobQueue.m_JobFlag.notify_all();
//
//		//	//m_JobQueue.m_LogMutex.lock();
//		//	pending_job->Execute();
//		//	delete pending_job;
//		//	//std::cout << " by thread " << id << std::endl;
//		//	//m_JobQueue.m_LogMutex.unlock();
//		//	m_JobQueue.m_CompleteJobsFlag.notify_one();
//		//}
//	}
//
//	m_JobQueue.m_LogMutex.lock();
//	std::cout << "____    Thread " << id << " done. _____" << std::endl;
//	m_JobQueue.m_LogMutex.unlock();
//}
//
//
//void JobQueue::Enqueue(Job* job)
//{
//
//	//lock queue condition variable from access
//	//when adding to the queue
//	std::lock_guard<std::mutex> queue_mutex(m_QueueMutex);
//
//	m_Queue.push_back(job);
//	//signal/notify threads waiting on flag
//	m_NotEmptyFlag.notify_one();
//	m_JobFlag.notify_one();
//}
//
//Job* JobQueue::Dequeue()
//{
//	//std::unique_lock<std::mutex> queue_mutex(m_QueueMutex);
//	////thread waits and when the condition variable m_NotEmptyFlag is signaled/notified 
//	////the argument/codition is checked  [this]() {return !m_Queue.empty(); 
//	//m_NotEmptyFlag.wait(queue_mutex, [this]() {return !m_Queue.empty(); });
//
//	Job* job = m_Queue.front();
//	m_Queue.pop_front();
//	return job;
//}
//
//bool JobQueue::IsEmpty()
//{
//	//std::lock_guard<std::mutex> queue_mutex(m_QueueMutex);
//	return m_Queue.empty();
//}
//
//
//void JobSystem::Update()
//{
//
//	/*JobFlag --> (notify all)
//	I Think this is a flag to notify that a job is avaliable to be exceuted.
//	And with notify all is to notify all threads with the flag (conditional variable).*/
//
//	std::unique_lock<std::mutex> queue_mutex(m_Queue.m_QueueMutex);
//	/* .wait condition_variable on the woek queue lock mutex
//	These puts this thread (main thread in this case) to sleep waiting for thread with the unique mutex
//	from workQueue.lock.*/
//	//recivce doneflag signal & the queue is done to move on
//	m_Queue.m_NotEmptyFlag.wait(queue_mutex, [&] {return m_Queue.IsEmpty(); });  //<----- This should block the main thread to wait for all othre thread
//
//
//	m_Queue.m_LogMutex.lock();
//	std::cout << "Thread Work finished !!!!!!!\n";
//	m_Queue.m_LogMutex.unlock();
//
//	//
//	///* done
//	//flag for program completion, which is shared by all threads. 
//	//That keeps them spinning/updating and operation their would stop when program is done*/
//	////sleep = true;
//
//	//for (size_t i = 0; i < m_NumWorker; ++i)
//	//	m_WorkerThread[i].join();
//
//
//
//
//
//
//#include <iostream>
//#include <mutex>
//#include <vector>
//#include <queue>
//
//class Job
//{
//public:
//	Job* next = nullptr;
//	virtual void Execute() = 0;
//};
//
//
//
//class JobQueue
//{
//public:
//	std::mutex m_QueueMutex;
//	std::mutex m_LogMutex;
//
//	std::condition_variable m_JobFlag;
//	std::condition_variable m_NotEmptyFlag;
//
//	void Enqueue(Job* job);
//	Job* Dequeue();
//	bool IsEmpty();
//
//private:
//	std::deque<Job*> m_Queue = std::deque<Job*>();
//};
//
//class Worker
//{
//private:
//	unsigned int id;
//	bool& m_SysActive;
//	JobQueue& m_JobQueue;
//
//public:
//	Worker(JobQueue& job_queue, bool& alive, unsigned int _id)
//		: m_JobQueue(job_queue), m_SysActive(alive), id(_id) {
//	}
//	void operator()();
//};
//
//class JobSystem
//{
//private:
//	std::vector<std::thread> m_WorkerThread;
//	unsigned int m_NumWorker = 0;
//
//	JobQueue m_Queue;
//	bool quit = false;
//
//public:
//	JobSystem() = default;
//	JobSystem(unsigned int num_worker) : m_NumWorker(num_worker)
//	{
//
//		m_WorkerThread.reserve(num_worker);
//		for (size_t i = 0; i < m_NumWorker; ++i)
//			m_WorkerThread.push_back(std::thread(Worker(m_Queue, quit, i)));
//	}
//
//
//	void NewJob(Job* job)
//	{
//		m_Queue.Enqueue(job);
//	}
//
//	void Update();
//
//	void Quit()
//	{
//		quit = true;
//		for (size_t i = 0; i < m_NumWorker; ++i)
//			m_WorkerThread[i].join();
//	}
//};