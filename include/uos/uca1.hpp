namespace uos {


struct uca1_t {
    struct waiting_task {
        volatile unsigned nr;
        const char * volatile data;
        volatile unsigned length;
        waiting_task * volatile next = nullptr;
    };

    void transmit(const char *data, unsigned length);
    //void transmit(const char *str);
    template<unsigned length>
    void transmit(const char(&str)[length]) {
        // add compile time string
        transmit(str, length-1);
    }

    waiting_task * volatile waiting_tx_;

    waiting_task *get_last_valid(waiting_task *task);

private:
    bool has_content(waiting_task *task);
    void remove_from_list(waiting_task &t);
};


extern uca1_t uca1;

}