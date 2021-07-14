// Code modified and based off of code examples on the course website

#include "systemc.h" 


// Simple constant generator. Works at least for builtin C types.
template <class T> SC_MODULE(DF_Const) { 
    sc_fifo_out<T> output;
    void process() { while (1) output.write(constant_); }
    SC_HAS_PROCESS(DF_Const); // needed as we do not use SC_CTOR(.)

    // constructor w/ module name and constant
    DF_Const(sc_module_name NAME, const T& CONSTANT) :
        sc_module(NAME), constant_(CONSTANT) { SC_THREAD(process); }

    T constant_; // the constant value we write to the output
}; 


// Simple dataflow adder. Works at least for builtin C types.
template <class T> SC_MODULE(DF_Adder) { 
    sc_in_clk clock;
    sc_in<T> input1, input2;
    sc_in<bool> ready;
    sc_out<bool> valid
    sc_out<T> output;

    void write(T x)
    {
        output.write(x);
        valid.write(true);
        do {
            wait(clock->posedge_event());
        } while (ready.read() != true);
        valid.write(false); // turn off valid after success
    }
    void process() { while (1) output.write(input1.read() + input2.read()); } 
    SC_CTOR(DF_Adder) { SC_THREAD(process); valid.initialize(false);} 
}; 


// Simple dataflow module that runs for a given number of iterations 
// (constructor argument) during which it prints the values read from
// its input on stdout. Works at least for builtin C types.
template <class T> SC_MODULE(DF_Printer) { 
    sc_fifo_in<T> input;
    SC_HAS_PROCESS(DF_Printer); // needed as we do not use SC_CTOR(.) 

    // constructor w/ name and number of iterations 
    DF_Printer(sc_module_name NAME, unsigned N_ITERATIONS) : 
        sc_module(NAME), n_iterations_(N_ITERATIONS)//, done_(false) 
        { SC_THREAD(process); } 

    void process() { 
        for (unsigned i=0; i<n_iterations_; i++) { 
            T value = input.read(); 
            cout << name() << " " << value << endl; 
        } 
//        done_ = true;
        sc_stop(); // terminate process after given # iterations 
    } 

    // destructor: check whether we have actually read a sufficient
    // number of values when the simulation ends.
//    ~DF_Printer() { 
//        if (!done_) cout << name() << " not done yet" << endl; 
//    }

    unsigned n_iterations_; // number of iterations
//    bool done_; // flag indicating whether we are done
}; 


// This module forks a dataflow stream.
template <class T> SC_MODULE(DF_Fork) {
    sc_fifo_in<T> input;
    sc_fifo_out<T> output1, output2;
    void process() {
        while(1) {
            T value = input.read();
            output1.write(value);
            output2.write(value);
        }
    }
    SC_CTOR(DF_Fork) { SC_THREAD(process); }
};


// Hardware FIFO class
template <class T> class hw_fifo : public sc_module
{
    public:
        sc_in_clk clock;
        sc_in<T> data_in;
        sc_in<bool> valid_in;
        sc_out<bool> ready_out;
        sc_out<T> data_out;
        sc_out<bool> valid_out;
        sc_in<bool> ready_in;

        unsigned _size, _first, _items;
        T* _data;

        SC_HAS_PROCESS(hw_fifo);

        hw_fifo(sc_module_name nm, unsigned size)
            : sc_module(nm), _size(size)
        {
            assert(size > 0);
            _first = _items = 0;
            _data = new T[_size];
            SC_CTHREAD(fifo_process, clk.pos());
            ready_out.initialize(true);
            valid_out.initialize(false);
        }

        ~hw_fifo() { delete[] _data; }

    protected:

        void fifo_process()
        {
          bool writable, readable;
            while(1) {
              writable = (_items < _size);
              readable = (_items > 0);
              if (valid_in.read() && writable)
              {
                // store new data item into fifo
                _data[(_first + _items) % _size] = data_in.read();
                ++_items;
              }
              if (ready_in.read() && readable)
              {
                // discard data item that was just read from fifo
                --_items;
                _first = (_first + 1) % _size;
              }
              // update all output signals
              ready_out.write(_items < _size);
              valid_out.write(_items > 0);
              data_out.write(_data[_first]);
                wait();
            }
        }
};


//Read adapter
template <class T> 
class FIFO_READ_HS 
  : public sc_module,
    public sc_fifo_in_if<T>
{
public:

    // ports
    sc_in_clk clock;
    sc_in<T> data;
    sc_in<bool> valid;
    sc_out<bool> ready;

    // blocking read
    void read(T& x) {
        // signal that we are ready to consume a token
        ready.write(true);
        // wait until we've got valid data
        do
            wait(clock->posedge_event());
        while (valid.read() != true);
        // read data
        x = data.read();
        // no more consumption for the moment
        ready.write(false);
    }

    // constructor
    SC_CTOR(FIFO_READ_HS) {
        ready.initialize(false);
    }

    // provide dummy implementations for unneeded
    // sc_fifo_out_if<T> methods:
    bool nb_read(T&) { assert(0); return false; }
    int num_available() const { assert(0); return 0; }
    const sc_event& data_written_event() const
    { static sc_event dummy; assert(0); return dummy; }

    virtual T read() {
        T tmp;
        read(tmp);
        return tmp;
    }

};

int sc_main(int argc, char* argv[]) 
{ 
    // module instances 
    DF_Const<int> constant("constant", 1);
    DF_Adder<int> adder("adder"); 
    DF_Fork<int> fork("fork");
    DF_Printer<int> printer("printer", 10);

    //channels
    FIFO_READ_HS<int> read_adapter("read_adapter");
    sc_clock clock("clock", 10, SC_NS);
    sc_signal<int> input_data("input_data");
    sc_signal<bool> input_ready("input_ready");
    sc_signal<bool> input_valid("input_valid");
    sc_signal<int> output_data("output_data");
    sc_signal<bool> output_ready("output_ready");
    sc_signal<bool> output_valid("output_valid");

    // fifos 
    sc_fifo<int> const_out("const_out", 1);
    //Replaced with HW fifo
    hw_fifo<int> adder_out("adder_out", 1);
    adder_out.clock(clock);
    adder_out.data_in(input_data);
    adder_out.valid_in(input_valid);
    adder_out.ready_in(input_ready);

    adder_out.data_out(output_data);
    adder_out.valid_out(output_valid);
    adder_out.ready_out(output_ready);

    read_adapter.clock(clock);
    read_adapter.data(output_data);
    read_adapter.ready(output_ready);
    read_adapter.valid(output_valid);
    sc_fifo<int> feedback("feedback", 1);
    sc_fifo<int> to_printer("to_printer", 1);

    // initial values
    feedback.write(40); // forget about this and the
                        // system will deadlock

    // interconnects 

    //hw fifo interconnects
    adder_out.clock(clock);
    adder_out.data_in(input_data);
    adder_out.valid_in(input_valid);
    adder_out.ready_in(input_ready);

    adder_out.data_out(output_data);
    adder_out.valid_out(output_valid);
    adder_out.ready_out(output_ready);

    // read adapter interconnects
    read_adapter.clock(clock);
    read_adapter.data(output_data);
    read_adapter.ready(output_ready);
    read_adapter.valid(output_valid);

    constant.output(const_out);

    // adder interconnects
    adder.clock(clock);
    adder.input1(const_out);
    adder.input2(feedback);
    adder.output(adder_out);
    adder.valid(input_valid);
    adder.ready(input_ready);

    fork.input(adder_out);
    fork.output1(feedback);
    fork.output2(to_printer);

    printer.input(to_printer);

    // Start simulation w/o time limit. The simulation will stop
    // when there are not more events. Once the printer module
    // has terminated the complete  simulation will eventually
    // come to a halt (after all fifos have been filled up) 
    sc_start(); 

    return 0; 
}