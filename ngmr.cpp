#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <functional>
#include <cmath>

using namespace std;

/**
 * Note: Use the vector to store the samples
 */
template <typename T, int dimension>
class Wine {
public:
    Wine(){numbers.resize(dimension);}
    Wine(string line, char delim = '#') {
        T ff;
        int pos_inline;
        char array[32] = {0}; 
        istringstream iss(line);
        numbers.resize(dimension);
        if (delim != '#') {
            while(iss.get(array, 32, delim)) {
                pos_inline = iss.tellg();
                iss.seekg(pos_inline + 1, ios::beg);
                numbers.push_back(atof(array));
            }
        }
        else 
            while (iss >> ff) numbers.push_back(ff);
        
        if (numbers.size() != dimension)
            cerr << "Wrong dimension of Wine in constructor.\n";
    }

    inline size_t size() { return numbers.size();}
    
    int nearest(vector<Wine> &winevector) {
        if (winevector.size() == 0) return -1;
        int nearest_idx = 0;
        double minimum_distance = HUGE_VALF;
        for (int i = 0; i < winevector.size(); i++) {
            double tmp_distance = (*this - winevector[i]);
            cout << "from " << *this << " to " << winevector[i] <<
                " is " << tmp_distance << endl;
            if (tmp_distance < minimum_distance) {
                nearest_idx = i;
                minimum_distance = tmp_distance;
            }
        }
        return nearest_idx;
    }

    friend inline double operator - (Wine &w1, Wine &w2) {
        if (w1.size() != w2.size()) 
           cerr << "Abstraction on different demension of wines.\n"; 
        Wine tmp_wine;
        for (int i = 0; i < w1.size(); i++)
            tmp_wine.numbers.push_back(w1.numbers[i] - w2.numbers[i]);
        return sqrt(inner_product(tmp_wine.numbers.begin(),
                    tmp_wine.numbers.end(), tmp_wine.numbers.begin(), 0.0));
    }
    friend inline ostream &operator << (ostream &oss, const Wine &wine){
        cout << "(";
        for (auto e : wine.numbers)
            oss << e << ", ";
        cout << ")";
        return oss;
    }
    // FIXME: Read a Wine from istream
    friend inline istream &operator >> (istream &iss, Wine &wine) {
        int pos_inline;
        char array[32];
        while(iss.get(array, 32, '\n')) {
            pos_inline = iss.tellg();
            iss.seekg(pos_inline + 1, ios::beg);
            wine.numbers.push_back(atof(array));
        }
        if (wine.size() != dimension)
            cerr << "Wrong dimension of Wine:" << wine.size() << endl;
        return iss;
    }

private:
    vector<T> numbers;
};

template<typename T>
class Dataset {
public:
    Dataset& load(string filename) {
        string line;
        ifstream fin(filename); 
        while (fin.good()) {
            getline(fin, line);
            elements.push_back(T(line, ','));
        }
        fin.close();
        return *this;
    }
    // TODO: the function should not modify the members of class
    // TODO: To performce the action through multi_thread
    Dataset& performance(function<void (T&)> const& action) {
        for_each(elements.begin(), elements.end(), action);
        return *this;
    }
    // TODO: shuffle function exchange data by MPI_Alltoall
    Dataset& shuffle() {return *this;}
    bool check() {return true;};
private:
    vector<T> elements;
};

int main()
{
    cout << "Hello, world.\n";
    typedef Wine<float, 2> T;
    string filename = "points.data";
    string line;
    T wine;
    vector<T> winevector;
    ifstream fin(filename);
    while(fin.good()) {
        getline(fin, line);
        if (line == "" || line[0] == '#')
            continue;
        T wine(line);
        winevector.push_back(wine);
    }

    cout << "winevector.size:" << winevector.size() << endl;
    int idx = winevector[2].nearest(winevector);
    cout << "The nearest point to " << winevector[2] << " is " <<
        winevector[idx];

//     typedef Wine<float, 14> T;
//     string filename = "wine.data";
//     Dataset<T> dataset;
//     dataset.load(filename);
//     dataset.shuffle();
//     
//     auto map    = [](T &x){cout << x << endl;};
//     auto reduce = [](T &x){};
//     do{
//         Dataset<T> ds1 = dataset.performance(map);
//         Dataset<T> ds2 = ds1.shuffle();
//         Dataset<T> ds3 = ds2.performance(reduce);
//         if (ds3.check() == true) break;
//     }while(true);

	return 0;
}
