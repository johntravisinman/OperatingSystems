/* times struct
 * a tuple to hold the start times and durations for child processes
 */
struct times
{
    int s;  //seconds
    int ns; //nanoseconds
    char dur[64]; //duration in ns
};