from multiprocessing.pool import ThreadPool
from primes import count_primes
from concurrent.futures import ThreadPoolExecutor
from time import perf_counter


def launch(workers: int, elements: int) -> tuple[bool, float]:
    values = [3000000] * elements

    start_t = perf_counter()
    with ThreadPoolExecutor(max_workers=workers) as tp:
        results = list(tp.map(count_primes, values))
    stop_t = perf_counter()

    is_ok = all(x == results[0] for x in results[1:])
    elapsed = stop_t - start_t
    return is_ok, elapsed


if __name__ == "__main__":
    workers = 4

    for i in range(1, 13):
        is_ok, elapsed = launch(workers, i)
        print(
            f"Launch with {workers} threads and {i} elements: {'Ok' if is_ok else 'Failed'} with time {elapsed:.6}s."
        )
