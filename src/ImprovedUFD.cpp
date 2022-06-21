//
// Created by lucas on 21/04/2022.
//

#include "ImprovedUFD.hpp"

#include "Decoder.hpp"
#include "TreeNode.hpp"

#include <cassert>
#include <chrono>
#include <queue>
#include <random>
#include <set>
/**
     * returns list of tree node (in UF data structure) representations for syndrome
     * @param code
     * @param syndrome
     * @return
    */
std::set<std::shared_ptr<TreeNode>> ImprovedUFD::computeInitTreeComponents(const gf2Vec& syndrome) {
    std::set<std::shared_ptr<TreeNode>> result{};
    for (std::size_t i = 0; i < syndrome.size(); i++) {
        if (syndrome.at(i)) {
            auto syndrNode     = code.tannerGraph.adjListNodes.at(i + code.getN()).at(0);
            syndrNode->isCheck = true;
            syndrNode->checkVertices.insert(syndrNode->vertexIdx);
            result.insert(syndrNode);
        }
    }
    return result;
}

// todo identify flagged errors?, i.e. when we do not find any error corresponding to the syndrome
void ImprovedUFD::decode(gf2Vec& syndrome) {
    std::chrono::steady_clock::time_point decodingTimeBegin = std::chrono::steady_clock::now();
    std::set<std::size_t>                 res;

    if (!syndrome.empty() && !std::all_of(syndrome.begin(), syndrome.end(), [](bool val) { return !val; })) {
        auto                                   syndrComponents = computeInitTreeComponents(syndrome);
        auto                                   components      = syndrComponents;
        std::vector<std::shared_ptr<TreeNode>> erasure;
        while (!components.empty()) {
            for (size_t i = 0; i < components.size(); i++) {
                // Step 1 growth
                std::vector<std::pair<std::size_t, std::size_t>> fusionEdges;
                std::map<std::size_t, bool>                      presentMap{}; // for step 4
                standardGrowth(fusionEdges, presentMap, components);

                // Step 2 Fusion of clusters
                auto eIt = fusionEdges.begin();
                while (eIt != fusionEdges.end()) {
                    auto n1    = code.tannerGraph.getNodeForId(eIt->first);
                    auto n2    = code.tannerGraph.getNodeForId(eIt->second);
                    auto root1 = TreeNode::Find(n1);
                    auto root2 = TreeNode::Find(n2);

                    //compares vertexIdx only
                    if (*root1 == *root2) {
                        eIt = fusionEdges.erase(eIt); // passes eIt to erase
                    } else {
                        std::size_t root1Size = root1->clusterSize;
                        std::size_t root2Size = root2->clusterSize;
                        TreeNode::Union(root1, root2);

                        if (root1Size <= root2Size) { // Step 3 fusion of boundary lists
                            for (auto& boundaryVertex: root1->boundaryVertices) {
                                root2->boundaryVertices.insert(boundaryVertex);
                            }
                            root1->boundaryVertices.clear();
                        } else {
                            for (auto& boundaryVertex: root2->boundaryVertices) {
                                root1->boundaryVertices.insert(boundaryVertex);
                            }
                            root2->boundaryVertices.clear();
                        }
                        eIt++;
                    }
                }

                // Step 4 Update roots avoiding duplicates
                auto it = components.begin();
                while (it != components.end()) {
                    auto elem = *it;
                    auto root = TreeNode::Find(elem);
                    if (!presentMap.contains(root->vertexIdx)) {
                        it = components.erase(it);
                        components.insert(root);
                    } else {
                        it++;
                    }
                }

                // Step 5 Update Boundary Lists, remove vertices that are not in boundary anymore
                for (auto& component: components) {
                    auto iter = component->boundaryVertices.begin();
                    while (iter != component->boundaryVertices.end()) {
                        auto nbrs     = code.tannerGraph.getNeighbours(*iter);
                        auto currNode = code.tannerGraph.getNodeForId(*iter);
                        auto currRoot = TreeNode::Find(currNode);
                        auto nbrIt    = nbrs.begin();
                        auto end      = nbrs.end();
                        while (nbrIt != end) {
                            auto nbr     = *nbrIt;
                            auto nbrRoot = TreeNode::Find(nbr);
                            if (currRoot->vertexIdx != nbrRoot->vertexIdx) {
                                // if we find one neighbour that is not in the same component the currNode is in the boundary
                                iter++;
                                break;
                            }
                            if (std::next(nbrIt) == end) {
                                // if we have checked all neighbours and found none in another component remove from boundary list
                                //toRemoveList.emplace_back(*iter);
                                iter = component->boundaryVertices.erase(iter);
                                break;
                            } else {
                                nbrIt++;
                            }
                        }
                    }
                }
                extractValidComponents(components, erasure);
            }
        }
        res = peelingDecoder(erasure, syndrComponents);
    }
    std::chrono::steady_clock::time_point decodingTimeEnd = std::chrono::steady_clock::now();
    this->result                                          = DecodingResult();
    result.decodingTime                                   = std::chrono::duration_cast<std::chrono::milliseconds>(decodingTimeEnd - decodingTimeBegin).count();
    result.estimBoolVector                                = gf2Vec(code.getN());
    for (auto re: res) {
        result.estimBoolVector.at(re) = true;
        result.estimNodeIdxVector.emplace_back(re);
    }
}

void ImprovedUFD::standardGrowth(std::vector<std::pair<std::size_t, std::size_t>>& fusionEdges,
                                 std::map<std::size_t, bool>& presentMap, const std::set<std::shared_ptr<TreeNode>>& components) {
    for (auto& component: components) {
        presentMap.insert(std::make_pair(component->vertexIdx, true));
        assert(component->parent == nullptr); // at this point we can assume that component represents root of the component
        auto bndryNodes = component->boundaryVertices;

        for (const auto& bndryNode: bndryNodes) {
            auto nbrs = code.tannerGraph.getNeighboursIdx(bndryNode);
            for (auto& nbr: nbrs) {
                fusionEdges.emplace_back(std::pair(bndryNode, nbr));
            }
        }
    }
}

void ImprovedUFD::singleClusterSmallestFirstGrowth(std::vector<std::pair<std::size_t, std::size_t>>& fusionEdges,
                                                   std::map<std::size_t, bool>& presentMap, const std::set<std::shared_ptr<TreeNode>>& components) {
    std::shared_ptr<TreeNode> smallestComponent;
    std::size_t               smallestSize = SIZE_MAX;
    for (const auto& c: components) {
        if (c->clusterSize < smallestSize) {
            smallestComponent = c;
        }
    }
    presentMap.insert(std::make_pair(smallestComponent->vertexIdx, true));
    assert(smallestComponent->parent == nullptr);
    auto bndryNodes = smallestComponent->boundaryVertices;

    for (const auto& bndryNode: bndryNodes) {
        auto nbrs = code.tannerGraph.getNeighboursIdx(bndryNode);
        for (auto& nbr: nbrs) {
            fusionEdges.emplace_back(std::pair(bndryNode, nbr));
        }
    }
}

void ImprovedUFD::singleClusterRandomFirstGrowth(std::vector<std::pair<std::size_t, std::size_t>>& fusionEdges,
                                                 std::map<std::size_t, bool>& presentMap, const std::set<std::shared_ptr<TreeNode>>& components) {
    std::shared_ptr<TreeNode>       chosenComponent;
    std::random_device              rd;
    std::mt19937                    gen(rd());
    gf2Vec                          result;
    std::uniform_int_distribution<> d(0U, components.size());
    std::size_t                     chosenIdx = d(gen);
    auto                            it        = components.begin();
    std::advance(it, chosenIdx);
    chosenComponent = *it;

    presentMap.insert(std::make_pair(chosenComponent->vertexIdx, true));
    assert(chosenComponent->parent == nullptr);
    auto bndryNodes = chosenComponent->boundaryVertices;

    for (const auto& bndryNode: bndryNodes) {
        auto nbrs = code.tannerGraph.getNeighboursIdx(bndryNode);
        for (auto& nbr: nbrs) {
            fusionEdges.emplace_back(std::pair(bndryNode, nbr));
        }
    }
}

//
//void ImprovedUF::orientedGroth(std::vector<std::pair<std::size_t, std::size_t>>& fusionEdges,
//                  std::map<std::size_t, bool>& presentMap, const std::set<std::shared_ptr<TreeNode>>& components) {
// todo
//}

/**
 * Computes interior of erasure with BFS algorithm then iterates over interior and removes neighbours of check vertices iteratively
 * @param erasure
 * @param syndrome
 * @return
 */
std::set<std::size_t> ImprovedUFD::erasureDecoder(std::vector<std::shared_ptr<TreeNode>>& erasure, std::set<std::shared_ptr<TreeNode>>& syndrome) {
    std::set<std::shared_ptr<TreeNode>> interior;
    std::vector<std::set<std::size_t>>  erasureSet{};
    std::size_t                         erasureSetIdx = 0;
    // compute interior of grown erasure components
    for (auto& currCompRoot: erasure) {
        std::set<std::size_t> compErasure;
        assert(currCompRoot->parent == nullptr); // should be due to steps above otherwise we can just call Find here
        std::queue<std::shared_ptr<TreeNode>> queue;

        // if currComp root is not in boundary, add it as first vertex to Int
        if (!currCompRoot->boundaryVertices.contains(currCompRoot->vertexIdx)) {
            currCompRoot->marked = true;
            compErasure.insert(currCompRoot->vertexIdx);
        }
        queue.push(currCompRoot);

        while (!queue.empty()) {
            auto currV = queue.front();
            queue.pop();

            std::vector<std::shared_ptr<TreeNode>> chldrn;
            for (auto& i: currV->children) {
                chldrn.emplace_back(i);
            }
            for (auto& node: chldrn) {
                if (!node->marked && !currCompRoot->boundaryVertices.contains(node->vertexIdx)) {
                    if (code.tannerGraph.getNeighbours(currV).contains(node)) {
                        currV->markedNeighbours.insert(node->vertexIdx);
                    }
                    // add to interior by adding it to the list and marking it
                    node->marked = true;
                    compErasure.insert(node->vertexIdx);
                    queue.push(node);
                }
            }
        }
        erasureSet.emplace_back(compErasure);
        erasureSetIdx++;
    }

    std::vector<std::set<std::size_t>> resList;
    // go through nodes in erasure
    // if current node v is a check node in Int, remove B(v, 1)
    for (auto& component: erasureSet) {
        std::set<std::size_t> xi;

        while (!syndrome.empty()) { //todo theres a bug somewhere here, does not terminate
            auto compNodeIt = component.begin();
            auto currN      = code.tannerGraph.getNodeForId(*compNodeIt);
            if (currN->marked && !currN->isCheck) {
                xi.insert(currN->vertexIdx);
                for (auto& markedNeighbour: currN->markedNeighbours) {
                    auto nNbrs = code.tannerGraph.getNeighboursIdx(markedNeighbour);
                    auto nnbr  = nNbrs.begin();
                    while (nnbr != nNbrs.end()) {
                        component.erase(*nnbr++);
                    }
                }
                auto ittt = currN->markedNeighbours.begin();
                while (ittt != currN->markedNeighbours.end()) {
                    component.erase(*ittt);
                    auto temp = code.tannerGraph.getNodeForId(*ittt);
                    assert(temp->isCheck);
                    syndrome.erase(temp);
                    ittt++;
                }
            } else {
                compNodeIt++;
            }
        }
        resList.emplace_back(xi);
    }
    std::set<std::size_t> res;
    for (auto& i: resList) {
        for (auto& n: i) {
            res.insert(n);
        }
    }
    return res;
}
/**
 * Computes spanning forest of erasure and peels iteratively, starting with boundary vertices
 * @param erasure
 * @param syndrome
 * @return
 */
std::set<std::size_t> ImprovedUFD::peelingDecoder(std::vector<std::shared_ptr<TreeNode>>& erasure, std::set<std::shared_ptr<TreeNode>>& syndrome) {
    std::set<std::size_t> erasureRoots;
    std::set<std::size_t> erasureVertices;
    for (auto& i: erasure) {
        erasureRoots.insert(i->vertexIdx);
    }
    std::set<std::size_t> syndr;
    for (const auto& s: syndrome) {
        syndr.insert(s->vertexIdx);
    }

    std::set<std::size_t> reslt;
    // compute SF
    std::vector<std::set<std::pair<std::size_t, std::size_t>>> spanningForest;
    std::vector<std::set<std::size_t>>                         forestVertices;
    std::set<std::size_t>                                      visited;
    for (auto& currCompRoot: erasureRoots) {
        std::queue<std::size_t>                       queue;
        std::set<std::pair<std::size_t, std::size_t>> tree;
        std::set<std::size_t>                         treeVertices;
        queue.push(currCompRoot);
        visited.insert(currCompRoot);
        treeVertices.insert(currCompRoot);
        while (!queue.empty()) {
            auto currV = queue.front();
            queue.pop();
            auto nbrs = code.tannerGraph.getNeighboursIdx(currV);
            for (auto& nbr: nbrs) {
                auto t1 = code.tannerGraph.getNodeForId(nbr);
                auto t2 = code.tannerGraph.getNodeForId(currV);
                if (!visited.contains(nbr)) {
                    //add all neighbours to edges to be able to identify pendant ones in next step
                    // nbrs we are looking at have to be in same erasure component, check with Find
                    if (TreeNode::Find(t1) == TreeNode::Find(t2)) {
                        visited.insert(nbr);
                        queue.push(nbr);
                        treeVertices.insert(nbr);
                        treeVertices.insert(currV);
                        tree.insert(std::make_pair(currV, nbr));
                    }
                }
            }
        }
        spanningForest.emplace_back(tree);
        forestVertices.emplace_back(treeVertices);
    }

    std::vector<std::set<std::size_t>> boundaryVertices;

    // compute pendant vertices of SF
    for (const auto& vertices: forestVertices) {
        std::set<std::size_t> pendants;
        for (auto& v: vertices) {
            auto nbrs = code.tannerGraph.getNeighboursIdx(v);
            for (auto& n: nbrs) {
                if (!vertices.contains(n)) { // if there is neighbour not in spanning tree
                    pendants.insert(v);
                }
            }
        }
        boundaryVertices.emplace_back(pendants);
    }

    // peeling
    std::size_t fNIdx = 0;
    for (auto& tree: spanningForest) {
        auto boundaryVtcs = boundaryVertices.at(fNIdx);
        while (!syndr.empty()) {
            auto edgeIt = tree.begin();
            while (edgeIt != tree.end()) {
                std::pair<std::size_t, std::size_t> e;
                std::size_t                         check;
                std::size_t                         data;
                if (code.tannerGraph.getNodeForId(edgeIt->first)->marked || code.tannerGraph.getNodeForId(edgeIt->first)->marked) {
                    edgeIt = tree.erase(edgeIt);
                    continue;
                }
                auto frst = edgeIt->first;
                auto scd  = edgeIt->second;
                // if in boundary simply remove
                if (boundaryVtcs.contains(frst)) {
                    code.tannerGraph.getNodeForId(frst)->marked = true;
                    forestVertices.at(fNIdx).erase(frst);
                } else if (boundaryVtcs.contains(scd)) {
                    code.tannerGraph.getNodeForId(scd)->marked = true;
                } else {
                    if (syndr.contains(frst)) {
                        check = frst;
                        data  = scd;
                    } else {
                        check = scd;
                        data  = frst;
                    }
                    // add data to estimate, remove check from syndrome
                    reslt.insert(data);
                    code.tannerGraph.getNodeForId(data)->marked  = true;
                    code.tannerGraph.getNodeForId(check)->marked = true;
                    forestVertices.at(fNIdx).erase(data);
                    forestVertices.at(fNIdx).erase(check);
                    syndr.erase(check);
                }
                edgeIt = tree.erase(edgeIt);

                // update boundary vertices
                boundaryVtcs.clear();
                for (auto& v: forestVertices.at(fNIdx)) {
                    auto nbrs = code.tannerGraph.getNeighbours(v);
                    for (auto& n: nbrs) {
                        if (!forestVertices.at(fNIdx).contains(n->vertexIdx) && !n->marked) {
                            boundaryVtcs.insert(v);
                        }
                    }
                }
            }
        }
        fNIdx++;
    }
    return reslt;
}

/**
 * Add those components that are valid to the erasure
 * @param components containts components to check validity for
 * @param erasure contains valid components (including possible new ones at end of function)
 */
void ImprovedUFD::extractValidComponents(std::set<std::shared_ptr<TreeNode>>& components, std::vector<std::shared_ptr<TreeNode>>& erasure) {
    auto it = components.begin();
    while (it != components.end()) {
        if (isValidComponent(*it)) {
            erasure.emplace_back(*it);
            it = components.erase(it);
        } else {
            it++;
        }
    }
}

// for each check node verify that there is no neighbour that is in the boundary of the component
// if there is no neighbour in the boundary for each check vertex the check is covered by a node in Int TODO prove this in paper
bool ImprovedUFD::isValidComponent(const std::shared_ptr<TreeNode>& component) {
    gf2Vec      valid(component->checkVertices.size());
    std::size_t i = 0;
    for (const auto& checkVertex: component->checkVertices) {
        auto nbrs = code.tannerGraph.getNeighbours(checkVertex);

        for (auto& nbr: nbrs) {
            if (std::find(component->boundaryVertices.begin(), component->boundaryVertices.end(), nbr->vertexIdx) == component->boundaryVertices.end()) {
                valid.at(i) = true;
                break;
            }
        }
        i++;
    }
    return std::all_of(valid.begin(), valid.end(), [](bool i) { return i; });
}
