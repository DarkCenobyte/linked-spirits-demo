/* LINKED//SPIRITS - director: time -> shot, camera, animation parameters */
#define TEXT_W 2048
#define TEXT_H 256
static int direct(float t, float *u)
{
    u[0] = t;
    u[NU * 4 - 1] = 0;
    return PR_LAB;
}
