#include <ilcplex/ilocplex.h>
#include <vector>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

// ===== Структуры данных =====
struct Node {
    int id;
    double x, y;
    double power;
    bool is_station;

    Node(int _id, double _x, double _y, double _p, bool _is)
        : id(_id), x(_x), y(_y), power(_p), is_station(_is) {
    }
};

struct Line {
    int from, to;
    double distance; // Длина
    double capacity; 
    double cost_per_km; // Стоимость

    Line(int f, int t, double d, double cap, double cost)
        : from(f), to(t), distance(d), capacity(cap), cost_per_km(cost) {
    }
};

vector<Node> nodes;
vector<Line> lines;

// ===== Вспомогательные функции =====
double calc_distance(double x1, double y1, double x2, double y2) {
    return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}

void create_data() {
    
    const vector<pair<double, double>> stations_coords = {
        {300, 100}, {600, 100}, {700, 200}, {900, 700}, {500, 500}, {400, 900}
    };

    const vector<double> stations_power = { 900, 500, 1200, 450, 750, 1200 };
    for (int i = 0; i < stations_coords.size(); i++) {
        nodes.push_back(Node(i, stations_coords[i].first, stations_coords[i].second, stations_power[i], true));
    }

    
    vector<pair<double, double>> cities_coords = {
        {150,100}, {400,80}, {950,70}, {30,120}, {600,300},
        {20,450}, {300,500}, {950,450}, {70,800}, {350,750},
        {500,750}, {600,800}, {600,900}, {750,750}, {850,950}
    };

    vector<double> cities_consumption = { 200,300,200,250,300,250,300,300,250,150,250,300,100,250,250 };
    for (int i = 0; i < cities_coords.size(); i++) {
        nodes.push_back(Node(nodes.size(), cities_coords[i].first, cities_coords[i].second, -cities_consumption[i], false));
    }

    // Создание линий (полный граф)
    for (int i = 0; i < nodes.size(); i++) {
        for (int j = i + 1; j < nodes.size(); j++) {
            double dist = calc_distance(nodes[i].x, nodes[i].y, nodes[j].x, nodes[j].y);

            lines.push_back(Line(
                i, j, dist,
                1000, // пропускная способность
                1e6   // стоимость
            ));
        }
    }
}


int main() {
    setlocale(LC_ALL, "Russian");
    IloEnv env;
    try {
        create_data();

        IloModel model(env);

        // 1. Объявление переменных
        IloIntVarArray y(env, lines.size(), 0, 2);  // Количество линий (0,1,2)
        IloNumVarArray f(env, lines.size(), 0, IloInfinity); // Поток мощности

        // 2. Целевая функция: минимизация стоимости
        IloExpr total_cost(env);
        for (int i = 0; i < lines.size(); i++) {
            total_cost += y[i] * lines[i].distance * lines[i].cost_per_km;
        }
        model.add(IloMinimize(env, total_cost));

        // 3. Ограничения
        // Баланс мощности для узлов
        for (const Node& node : nodes) {
            IloExpr inflow(env), outflow(env);
            for (int i = 0; i < lines.size(); i++) {
                if (lines[i].to == node.id) inflow += f[i];
                if (lines[i].from == node.id) outflow += f[i];
            }
            if (node.is_station) {
                model.add(outflow - inflow <= node.power); // Ограничение генерации
            }
            else {
                model.add(inflow - outflow == -node.power); // Ограничение потребления
            }
            inflow.end();
            outflow.end();
        }

        // Пропускная способность
        for (int i = 0; i < lines.size(); i++) {
            model.add(f[i] <= lines[i].capacity * y[i]);
        }

        // Надёжность: города должны иметь ≥2 подключений
        for (const Node& node : nodes) {
            if (!node.is_station) {
                IloExpr connections(env);
                for (int i = 0; i < lines.size(); i++) {
                    if (lines[i].from == node.id || lines[i].to == node.id) {
                        connections += y[i];
                    }
                }
                model.add(connections >= 2);
                connections.end();
            }
        }
        IloCplex cplex(model);
        cplex.setOut(env.getNullStream());
        cplex.setWarning(env.getNullStream());
        cplex.setError(env.getNullStream());

        // 4. Решение задачи
        cplex.solve();

        // 5. Вывод результатов
        if (cplex.getStatus() == IloAlgorithm::Optimal) {
            cout << "Минимальная стоимость: " << cplex.getObjValue() / 1e6 << " млн евро" << endl;
            cout << "Активные линии:" << endl;
            for (int i = 0; i < lines.size(); i++) {
                if (cplex.getValue(y[i]) > 0) {
                    cout << "Узел " << lines[i].from << " -> " << lines[i].to
                        << ", линии: " << cplex.getValue(y[i])
                        << ")" << endl;
                }
            }
        }
        else {
            cout << "Оптимальное решение не найдено!" << endl;
        }

    }
    catch (IloException& e) {
        cerr << "Ошибка CPLEX: " << e.getMessage() << endl;
    }

    env.end();
    return 0;
}