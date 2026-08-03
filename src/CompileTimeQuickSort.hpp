#pragma once
#include <array>

enum class FileTransferMode;
struct FileType;

//a constexpr friendly implementation of quicksort so that the array of filetypes can be sorted at compile time
template<size_t T>
constexpr int partition(std::array<FileType, T>& arr, int lowIndex, int highIndex)
{
	FileType pivot{ arr[highIndex] };
	FileType temp;
	int i{ lowIndex - 1 };
	for (int j{ lowIndex }; j < highIndex; ++j)
	{
		if (arr[j] < pivot)
		{
			++i;
			temp = arr[i];
			arr[i] = arr[j];
			arr[j] = temp;
		}
	}

	temp = arr[i + 1];
	arr[i + 1] = arr[highIndex];
	arr[highIndex] = temp;
	return i + 1;
}

template<size_t T>
constexpr void quickSort(std::array<FileType, T>& arr, int lowIndex, int highIndex)
{
	if (highIndex > lowIndex)
	{
		int part{ partition(arr, lowIndex, highIndex) };
		quickSort(arr, lowIndex, part - 1);
		quickSort(arr, part + 1, highIndex);
	}
}
