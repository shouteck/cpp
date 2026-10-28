
const int N = 512;

int buf[N];

int head = 0;
int tail = 0;

bool push(int x) {

	if ((tail + 1) % N == head) return 0;
	buf[tail] = x;
	tail = (tail + 1) % N;

	return 1;
}

bool pop(int& x) {

	if (head == tail) return 0;
	x = buf[head];
	head = (head + 1) % N;

	return 1;

}

int main() {

	return 0;

}
