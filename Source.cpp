//вариант кода без точек штейнера (создает файлы для основного решения с рабочими станциями, попытка решения при отключенной станции 0-5)
//пример вывода данный
//2 5 1 270.3
//1 линия от узла 2 к узлу 5, по которой идет 270.3МВт
#include <ilcplex/ilocplex.h>
#include <vector>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

// Структура узла: может быть городом (потребление) или электростанцией (генерация)
struct Node {
    int id;
    double x, y;
    double power;     // > 0 для станции, < 0 для города
    bool is_station;  // true = станция, false = город

    Node(int _id, double _x, double _y, double _p, bool _is)
        : id(_id), x(_x), y(_y), power(_p), is_station(_is) {
    }
};

// Структура линии: соединяет два узла, имеет стоимость, длину и пропускную способность
struct Line {
    int from, to;
    double distance;
    double capacity;
    double cost_per_km;

    Line(int f, int t, double d, double cap, double cost)
        : from(f), to(t), distance(d), capacity(cap), cost_per_km(cost) {
    }
};

vector<Node> nodes;
vector<Line> lines;

// Загрузка данных из заранее заданных файлов
void read_data_from_files() {
    nodes.clear();
    lines.clear();

    string plant_path = "./power_plant.txt";
    string home_path = "./home.txt";

    ifstream plant_file(plant_path);
    if (!plant_file.is_open()) {
        cerr << "Error: cannot open " << plant_path << endl;
        exit(1);
    }

    ifstream home_file(home_path);
    if (!home_file.is_open()) {
        cerr << "Error: cannot open " << home_path << endl;
        exit(1);
    }

    // Чтение электростанций
    int num_plants;
    plant_file >> num_plants;
    for (int i = 0; i < num_plants; ++i) {
        double x, y, p;
        plant_file >> x >> y >> p;
        nodes.emplace_back(nodes.size(), x, y, p, true);
    }
    plant_file.close();

    // Чтение городов
    int num_cities;
    home_file >> num_cities;
    for (int i = 0; i < num_cities; ++i) {
        double x, y, demand;
        home_file >> x >> y >> demand;
        nodes.emplace_back(nodes.size(), x, y, -demand, false);
    }
    home_file.close();

    // Генерация всех возможных расстояний между узлами на расстоянии ≤ 1000 км
    const double MAX_DIST = 1000.0;
    for (int i = 0; i < nodes.size(); ++i) {
        for (int j = i + 1; j < nodes.size(); ++j) {
            double dx = nodes[i].x - nodes[j].x;
            double dy = nodes[i].y - nodes[j].y;
            double dist = sqrt(dx * dx + dy * dy);
            if (dist <= MAX_DIST) {
                lines.emplace_back(i, j, dist, 1000, 1e6);  // 1000 МВт, 1 млн €/км
            }
        }
    }
}

// Решение задачи оптимизации для заданного набора узлов
bool solve_model(IloEnv& env, const vector<Node>& local_nodes, const string& tag = "") {
    try {
        IloModel model(env);
        IloIntVarArray y(env, lines.size(), 0, 2);               // Кол-во линий (0–2)
        IloNumVarArray f(env, lines.size(), 0, IloInfinity);     // Потоки по линиям

        // Целевая функция: минимизация стоимости всех линий
        IloExpr cost(env);
        for (int i = 0; i < lines.size(); ++i) {
            cost += y[i] * lines[i].distance * lines[i].cost_per_km;
        }
        model.add(IloMinimize(env, cost));

        // Ограничения баланса мощности (вход - выход = потребление/генерация)
        for (const Node& node : local_nodes) {
            IloExpr inflow(env), outflow(env);
            for (int i = 0; i < lines.size(); ++i) {
                if (lines[i].to == node.id) inflow += f[i];
                if (lines[i].from == node.id) outflow += f[i];
            }
            if (node.is_station)
                model.add(outflow - inflow <= node.power);
            else
                model.add(inflow - outflow == -node.power);
        }

        // Ограничения пропускной способности линий
        for (int i = 0; i < lines.size(); ++i) {
            model.add(f[i] <= lines[i].capacity * y[i]);
        }

        // Минимум два подключения для каждого города
        for (const Node& node : local_nodes) {
            if (!node.is_station) {
                IloExpr conn(env);
                for (int i = 0; i < lines.size(); ++i) {
                    if (lines[i].from == node.id || lines[i].to == node.id) {
                        conn += y[i];
                    }
                }
                model.add(conn >= 2);
            }
        }

        // Запуск решателя
        IloCplex cplex(model);
        cplex.setOut(env.getNullStream());  // Без вывода в консоль
        if (!cplex.solve()) {
            cout << "Solution not found for case: " << tag << endl;
            return false;
        }

        // Сохранение результата в файл
        ofstream out("solution_" + tag + ".txt");
        for (int i = 0; i < lines.size(); ++i) {
            if (cplex.getValue(y[i]) > 0.5) {
                out << lines[i].from << " " << lines[i].to << " "
                    << cplex.getValue(y[i]) << " " << cplex.getValue(f[i]) << endl;
            }
        }
        out.close();

        cout << "Solution found for case: " << tag
            << ". Total cost: " << cplex.getObjValue() / 1e6 << " million euros." << endl;
        return true;
    }
    catch (IloException& e) {
        cerr << "CPLEX error: " << e.getMessage() << endl;
        return false;
    }
}

// Проверка надёжности: моделируем отказ каждой станции
bool check_reliability(IloEnv& env) {
    cout << "\n=== Reliability Check: Simulating single station failures ===" << endl;
    bool all_ok = true;

    for (int i = 0; i < nodes.size(); ++i) {
        if (!nodes[i].is_station) continue;

        vector<Node> modified_nodes = nodes;
        modified_nodes[i].power = 0;  // Simulate failure of power plant i

        // Начальное ограничение на расстояние соединения
        double max_dist = 1000.0;

        int attempts = 0;
        bool success = false;

        while (!success && attempts < 5) {
            // На каждой итерации создаём новый список линий с увеличенным радиусом
            vector<Line> extended_lines;
            for (int u = 0; u < nodes.size(); ++u) {
                for (int v = u + 1; v < nodes.size(); ++v) {
                    double dx = nodes[u].x - nodes[v].x;
                    double dy = nodes[u].y - nodes[v].y;
                    double dist = sqrt(dx * dx + dy * dy);
                    if (dist <= max_dist) {
                        extended_lines.emplace_back(u, v, dist, 1000, 1e6);
                    }
                }
            }

            lines = extended_lines;  // Подменяем глобальные линии
            string tag = "no_station_" + to_string(i) + "_try" + to_string(attempts);
            success = solve_model(env, modified_nodes, tag);

            if (!success) {
                max_dist += 100.0;  // Увеличиваем радиус на следующую попытку
                attempts++;
            }
        }

        if (!success) {
            cout << "Network is NOT reliable when power plant " << i
                << " is offline. Attempts made: " << attempts << endl;
            all_ok = false;
        }
        else {
            cout << "Network passed reliability test for power plant " << i
                << " after " << attempts << " attempts." << endl;
        }
    }

    return all_ok;
}

int main() {
    setlocale(LC_ALL, "C");
    IloEnv env;

    read_data_from_files();                          // Загрузка данных
    solve_model(env, nodes, "base");                 // Базовое решение
    check_reliability(env);                          // Надёжность при отказе станций

    env.end();
    return 0;
}
