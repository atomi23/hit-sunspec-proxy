#pragma once

template <int N>
class ClientActivity {
  int order[N] = {};

public:
  constexpr ClientActivity() {
    for (int i = 0; i < N; i++) order[i] = i;
  }

  int oldest() const { return order[0]; }

  void touch(int slot) {
    int i = 0;
    while (order[i] != slot) i++;
    for (; i < N - 1; i++) order[i] = order[i + 1];
    order[N - 1] = slot;
  }
};
