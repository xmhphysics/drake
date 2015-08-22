
#include <algorithm>
#include "DrakeSystem.h"



using namespace std;

DrakeSystem::DrakeSystem(const std::string &_name, const std::shared_ptr<CoordinateFrame>& _continuous_state_frame,
                         const std::shared_ptr<CoordinateFrame>& _discrete_state_frame,
                         const std::shared_ptr<CoordinateFrame>& _input_frame,
                         const std::shared_ptr<CoordinateFrame>& _output_frame)
  : name(_name),
    continuous_state_frame(_continuous_state_frame),
    discrete_state_frame(_discrete_state_frame),
    input_frame(_input_frame),
    output_frame(_output_frame) {
  if (!continuous_state_frame) continuous_state_frame = make_shared<CoordinateFrame>(name+"ContinuousState");
  if (!discrete_state_frame) discrete_state_frame = make_shared<CoordinateFrame>(name+"DiscreteState");
  if (!input_frame) input_frame = make_shared<CoordinateFrame>(name+"Input");
  if (!output_frame) output_frame = make_shared<CoordinateFrame>(name+"Output");
  state_frame = make_shared<MultiCoordinateFrame>(name+"State",initializer_list<CoordinateFramePtr>({continuous_state_frame,discrete_state_frame}));
}

DrakeSystem::DrakeSystem(const std::string &_name, unsigned int num_continuous_states, unsigned int num_discrete_states,
                         unsigned int num_inputs, unsigned int num_outputs)
        : name(_name),
          input_frame(nullptr),
          continuous_state_frame(nullptr),
          discrete_state_frame(nullptr),
          output_frame(nullptr) {
  input_frame = make_shared<CoordinateFrame>(name+"Input",num_inputs,"u");
  continuous_state_frame = make_shared<CoordinateFrame>(name+"ContinuousState",num_continuous_states,"xc");
  discrete_state_frame = make_shared<CoordinateFrame>(name+"DiscreteState",num_discrete_states,"xd");
  output_frame = make_shared<CoordinateFrame>(name+"Output",num_outputs,"y");
  state_frame = make_shared<MultiCoordinateFrame>(name+"State",initializer_list<CoordinateFramePtr>({continuous_state_frame,discrete_state_frame}));
}

DrakeSystem::VectorXs DrakeSystem::getRandomState(void) {
  return DrakeSystem::VectorXs::Random(state_frame->getDim());
}

DrakeSystem::VectorXs DrakeSystem::getInitialState(void) {
  return getRandomState();
}

void DrakeSystem::simulate(double t0, double tf, const DrakeSystem::VectorXs &x0, const SimulationOptions& options) {
  ode1(t0,tf,x0,options);
}

void DrakeSystem::runLCM(double t0, double tf, const VectorXs &x0, const SimulationOptions& options) {
  DrakeSystemPtr lcm_sys = output_frame->setupLCMOutputs(shared_from_this());

  bool has_lcm_input = false;
  {
    DrakeSystemPtr new_sys = input_frame->setupLCMInputs(lcm_sys);
    has_lcm_input = (new_sys != lcm_sys);
    lcm_sys = new_sys;
  }

  if (has_lcm_input && state_frame->getDim()<1) {
    // then this is really a static function, not a dynamical system.
    // block on receiving lcm input and process the output exactly when a new input message is received.

    throw runtime_error("not implemented yet (but will be simple)");

    // todo: lcm handle loop goes here

  } else {
    // then the system's dynamics should control when the output is produced.  try to run a simulation at realtime

    SimulationOptions lcm_options = options;
    lcm_options.realtime_factor = 1.0;

    if (has_lcm_input) {
      throw runtime_error("not implemented yet");
      // todo: spawn a thread for the lcm handle loop
    }

    lcm_sys->simulate(t0,tf,x0,lcm_options);

    if (has_lcm_input) {
      // todo: shutdown the lcm handle thread
    }
  }
}


#include "timeUtil.h"

inline void handle_realtime_factor(double wall_clock_start_time, double sim_time, double realtime_factor)
{
  if (realtime_factor>=0.0) {
    double time_diff = (getTimeOfDay() - wall_clock_start_time) * realtime_factor - sim_time;
    if (time_diff <= 0.0) {
      nanoSleep(-time_diff);
    } else if (time_diff > 1.0) { // then I'm behind by more than 1 second
      throw runtime_error(
              "Not keeping up with requested realtime_factor!  At time " + to_string(sim_time) + ", I'm behind by " +
              to_string(time_diff) + " sec.");
    }
  }
}


void DrakeSystem::ode1(double t0, double tf, const DrakeSystem::VectorXs& x0, const SimulationOptions& options) {
  double t = t0, dt;
  double wall_clock_start_time = getTimeOfDay();
  DrakeSystem::VectorXs x = x0;
  DrakeSystem::VectorXs u = DrakeSystem::VectorXs::Zero(input_frame->getDim());
  DrakeSystem::VectorXs y(output_frame->getDim());
  while (t<tf) {
    handle_realtime_factor(wall_clock_start_time, t, options.realtime_factor);

    dt = std::min(options.initial_step_size,tf-t);
    y = output(t,x,u);
    x += dt * dynamics(t, x, u);
    t += dt;
  }
}



CascadeSystem::  CascadeSystem(const DrakeSystemPtr& _sys1, const DrakeSystemPtr& _sys2)
  : DrakeSystem("CascadeSystem"), sys1(_sys1), sys2(_sys2) {
  if (sys1->output_frame != sys2->input_frame)
    throw runtime_error("Cascade combination failed: output frame of "+sys1->name+" must match the input frame of "+sys2->name);
  input_frame = sys1->input_frame;
  output_frame = sys2->output_frame;
  continuous_state_frame = make_shared<MultiCoordinateFrame>("CascadeSystemContState",initializer_list<CoordinateFramePtr>({sys1->continuous_state_frame, sys2->continuous_state_frame}));
  discrete_state_frame = make_shared<MultiCoordinateFrame>("CascadeSystemDiscState",initializer_list<CoordinateFramePtr>({sys1->discrete_state_frame, sys2->discrete_state_frame}));
  state_frame = make_shared<MultiCoordinateFrame>("CascadeSystemState",initializer_list<CoordinateFramePtr>({continuous_state_frame,discrete_state_frame}));
}

DrakeSystem::VectorXs CascadeSystem::getX1(const VectorXs &x) {
  DrakeSystem::VectorXs x1(sys1->state_frame->getDim());
  x1 << x.head(sys1->continuous_state_frame->getDim()),
        x.segment(sys1->state_frame->getDim(),sys1->discrete_state_frame->getDim());
  return x1;
}

DrakeSystem::VectorXs CascadeSystem::getX2(const VectorXs &x) {
  DrakeSystem::VectorXs x2(sys2->state_frame->getDim());
  x2 << x.segment(sys1->continuous_state_frame->getDim(),sys2->continuous_state_frame->getDim()),
        x.segment(sys1->state_frame->getDim()+sys2->continuous_state_frame->getDim(),sys2->discrete_state_frame->getDim());
  return x2;
}

DrakeSystem::VectorXs CascadeSystem::dynamics(double t, const DrakeSystem::VectorXs& x, const DrakeSystem::VectorXs& u) {
  unsigned int num_xc1 = sys1->continuous_state_frame->getDim(),
               num_xc2 = sys2->continuous_state_frame->getDim();

  DrakeSystem::VectorXs x1 = getX1(x);
  DrakeSystem::VectorXs y1 = sys1->output(t,x1,u);
  DrakeSystem::VectorXs xdot(continuous_state_frame->getDim());
  if (num_xc1>0) xdot.head(num_xc1) = sys1->dynamics(t,x1,u);
  if (num_xc2>0) xdot.tail(num_xc2) = sys2->dynamics(t,getX2(x),y1);
  return xdot;
}

DrakeSystem::VectorXs CascadeSystem::update(double t, const DrakeSystem::VectorXs& x, const DrakeSystem::VectorXs& u) {
  unsigned int num_xd1 = sys1->discrete_state_frame->getDim(),
          num_xd2 = sys2->discrete_state_frame->getDim();

  DrakeSystem::VectorXs x1 = getX1(x);
  DrakeSystem::VectorXs y1 = sys1->output(t,x1,u);
  DrakeSystem::VectorXs xn(discrete_state_frame->getDim());
  if (num_xd1>0) xn.head(num_xd1) = sys1->update(t,x1,u);
  if (num_xd2>0) xn.tail(num_xd2) = sys2->update(t,getX2(x),y1);
  return xn;
}

DrakeSystem::VectorXs CascadeSystem::output(double t, const DrakeSystem::VectorXs& x, const DrakeSystem::VectorXs& u) {
  DrakeSystem::VectorXs y1 = sys1->output(t,getX1(x),u);
  return sys2->output(t,getX2(x),y1);
}
