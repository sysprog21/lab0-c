# Test performance of 'q_sort' with random and descending orders: 'q_new', 'q_free', 'q_insert_head', 'q_sort', and 'q_reverse'
# 10000: all correct sorting algorithms are expected pass
# 50000: sorting algorithms with O(n^2) time complexity are expected failed
# 100000: sorting algorithms with O(nlogn) time complexity are expected pass
option fail 0
option malloc 0
new
ih RAND 10000
sort
reverse
sort
free
new
ih RAND 50000
sort
reverse
sort
free
new
ih RAND 100000
sort
reverse
sort
free
