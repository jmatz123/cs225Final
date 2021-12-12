#include "graph_visual.h"
#include <algorithm>
#include <cmath>
#include "utils.h"

using namespace std;
using namespace cs225;

GraphVisual::GraphVisual() {}

GraphVisual::GraphVisual(Graph graph, unsigned width, unsigned height) {
  // steal adjacency matrix
  // create node from streamers
  g_ = graph;
  width_ = width;
  height_ = height;

  vector<Streamer> streamers = g_.getStreamers();

  for (Streamer s : streamers) {
    unsigned randX = rand() % (width + 1);
    unsigned randY = rand() % (height + 1);
    pair<unsigned, unsigned> coords = make_pair(randX, randY);

    // modify this for size of node, viewers = e^radius - 1
    unsigned radius = kRadiusGrouping[kRadiusGrouping.size() - 1].second;
    unsigned hue = 0;
    // ((kUpperBound * (s.getViews() - kMinViews)) / (kMaxViews - kMinViews)) +
    // kShift; 2 * log(min(s.getViews(), kClipValue) + 1);  // standardizes
    // radius based on range of views */
    for (unsigned i = 1; i < kRadiusGrouping.size(); i++) {
      if (s.getViews() < kRadiusGrouping[i].first) {
        radius = kRadiusGrouping[i - 1].second;
        hue = kHueVector[i - 1];
        break;
      }
    }
    Node n(
        radius, coords, s,
        hue);  // creates a node for each streamer with random intial position

    nodes_.push_back(n);
  }

  adjMatrix_ = g_.getAdjMatrix();

  forceConst_ =
      kAreaConst * (sqrt((width_ * height_) / g_.getStreamers().size()));
}

float GraphVisual::CalcAngle(pair<unsigned, unsigned> thisPoint,
                             pair<unsigned, unsigned> otherPoint) {
  int delX = thisPoint.first - otherPoint.first;
  int delY = thisPoint.second - otherPoint.second;

  float angleDegrees = atan2(delY, delX) * 180 / M_PI;

  return angleDegrees;
}

pair<double, double> GraphVisual::CalcComponents(double force, float angleDeg) {
  double xComp = force * cos(angleDeg * M_PI / 180);
  double yComp = force * sin(angleDeg * M_PI / 180);

  return make_pair(xComp, yComp);
}
double GraphVisual::CalcDistance(Node n1, Node n2) {
  unsigned x = n1.center.first - n2.center.first;
  unsigned y = n1.center.second - n2.center.second;

  double dist = sqrt(pow(x, 2) + pow(y, 2));

  return dist;
}

pair<double, double> GraphVisual::CalcAttractionForce(Node n1, Node n2) {
  double distance = CalcDistance(n1, n2);

  double a_force = pow(distance, 2) / forceConst_;

  float angle = CalcAngle(n1.center, n2.center);

  pair<double, double> components = CalcComponents(a_force, angle);

  return components;
}

pair<double, double> GraphVisual::CalcRepulsionForce(Node n1, Node n2) {
  double distance = CalcDistance(n1, n2);

  double a_force = -1 * pow(forceConst_, 2) / distance;

  float angle = CalcAngle(n1.center, n2.center);

  pair<double, double> components = CalcComponents(a_force, angle);

  return components;
}

void GraphVisual::Arrange() {
  unsigned count = 0;
  double sumDisplacement = INT_MAX;
  while (count < kMaxIterations && sumDisplacement > KDisplaceThreshold) {
    sumDisplacement = 0;
    vector<pair<double, double>> nodeVelocities;
    for (unsigned i = 0; i < nodes_.size(); i++) {
      vector<pair<double, double>> netForce;
      for (unsigned j = 0; j < nodes_.size(); j++) {
        if (i != j) {
          netForce.push_back(CalcRepulsionForce(nodes_[i], nodes_[j]));
        }
        if (adjMatrix_[nodes_[i].streamer.getId()][nodes_[i].streamer.getId()] >
            0) {
          netForce.push_back(CalcAttractionForce(nodes_[i], nodes_[j]));
        }
      }
      double xNet = 0;
      double yNet = 0;
      for (pair<double, double> p : netForce) {  // sum up the net force
        xNet += p.first;
        yNet += p.second;
      }

      nodeVelocities.push_back(make_pair(xNet, yNet));
    }
    for (unsigned v = 0; v < nodeVelocities.size(); v++) {
      if (nodeVelocities[v].first < 0) {
        nodes_[v].center.first -= nodeVelocities[v].first;
      } else {
        nodes_[v].center.first += nodeVelocities[v].first;
      }

      if (nodeVelocities[v].second < 0) {
        nodes_[v].center.second -= nodeVelocities[v].second;
      } else {
        nodes_[v].center.second += nodeVelocities[v].second;
      }
    }

    for (pair<double, double> p : nodeVelocities) {
      sumDisplacement += sqrt(pow(p.first, 2) + pow(p.second, 2));
    }
    count++;
  }

  int largest_negX = 0;
  int largest_negY = 0;

  for (unsigned i = 0; i < nodes_.size(); i++) {
    if (nodes_[i].center.first < largest_negX) {
      largest_negX = nodes_[i].center.first;
    }

    if (nodes_[i].center.second < largest_negY) {
      largest_negY = nodes_[i].center.second; 
    }
  }

  for (unsigned i = 0; i < nodes_size(); i++) {
    nodes_[i].center.first -= largest_negX;
    nodes[i].center.second -= largest_negY;
  }

}

void GraphVisual::drawEdge(Node n1, Node n2, PNG& png) {
  unsigned x1 = n1.center.first;
  unsigned x2 = n2.center.first;
  unsigned y1 = n1.center.second;
  unsigned y2 = n2.center.second;

  if ((x1 == 0 && y1 == 0) || (x2 == 0 && y2 == 0)) return;

  if (x2 < x1) {
    swap(x1, x2);
    swap(y1, y2);
  }

  double slope = static_cast<double>(y2 - y1) / (x2 - x1);

  for (unsigned x = x1; x < x2; x++) {
    unsigned y = slope*(x - x1) + y1;
    HSLAPixel& p = png.getPixel(x, y);
    p.l = 0;
  }
}
void GraphVisual::drawAllEdges(PNG& png) {
  for (unsigned x = 0; x < adjMatrix_.size(); x++) {
    for (unsigned y = 0; y < adjMatrix_[x].size(); y++) {
      if (g_.isAdjacent(x, y) > 0) {
        drawEdge(nodes_[x], nodes_[y], png);
      }
    }
  }
}

void GraphVisual::drawNode(Node n, PNG& png) {
  unsigned rect_x1 = n.center.first - n.radius;
  unsigned rect_y1 = n.center.second - n.radius;
  unsigned rect_x2 = n.center.first + n.radius;
  unsigned rect_y2 = n.center.second + n.radius;

  for (unsigned x = rect_x1; x < rect_x2; x++) {
    for (unsigned y = rect_y1; y < rect_y2; y++) {
      pair<unsigned, unsigned> curr = make_pair(x, y);
      double dist = utils::distance(curr, n.center);
      if (dist < n.radius) {
        HSLAPixel& p = png.getPixel(x, y);
        p.h = n.hue;
        p.s = 1;
        p.l = 0.5;
        p.a = 1;
      }
    }
  }

  /* Square around center for testing
  for (unsigned x = n.center.first - 2; x < n.center.first + 2; x++) {
    for (unsigned y = n.center.second - 2; y < n.center.second + 2; y++) {
      HSLAPixel& p = png.getPixel(x, y);
      p.h = 319;
      p.s = 1;
      p.l = 0.5;
      p.a = 1;
    }
  }
  */
}

vector<GraphVisual::Node> GraphVisual::getNodes() { return nodes_; }