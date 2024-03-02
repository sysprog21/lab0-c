#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np
counterSet = {'1234': 41786, '1243': 41534, '1324': 41652, '1342': 41614, '1423': 41678, '1432': 41603, '2134': 41851, '2143': 41566, '2314': 42071, '2341': 41363, '2413': 41564, '2431': 41643, '3124': 41423, '3142': 41935, '3214': 41502, '3241': 41731, '3412': 41743, '3421': 41694, '4123': 41688, '4132': 41634, '4213': 41758, '4231': 41770, '4312': 41552, '4321': 41645}
permutations = [number for number in range(1,len(counterSet)+1)]
counts = counterSet.values()

x = np.arange(len(counts))
plt.bar(x, counts)
plt.xticks(x, permutations)
plt.xlabel('permutations')
plt.ylabel('counts')
plt.title('Shuffle result')
plt.savefig('./result.png') 
plt.show()