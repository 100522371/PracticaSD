struct log_args{
    string username<50>;
    string op<1024>;
    string filename<256>;
};

program LOGGING{
    version LOGVER{
        int log(log_args) = 1;
    } = 1;
} = 100522371;
