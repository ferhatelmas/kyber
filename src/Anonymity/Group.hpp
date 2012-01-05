#ifndef DISSENT_ANONYMITY_GROUP_H_GUARD
#define DISSENT_ANONYMITY_GROUP_H_GUARD

#include <algorithm>

#include <QDataStream>
#include <QHash>
#include <QMetaEnum>
#include <QSharedData>
#include <QSharedPointer>
#include <QVector>

#include "Connections/Id.hpp"
#include "Crypto/NullPrivateKey.hpp"
#include "Utils/Triple.hpp"

namespace Dissent {
namespace Crypto {
  class AsymmetricKey;
}

namespace Anonymity {
  typedef Dissent::Utils::Triple<Dissent::Connections::Id,
          QSharedPointer<Dissent::Crypto::AsymmetricKey>,
          QByteArray> GroupContainer;

  /**
   * Private data structure for Group storage.
   */
  class GroupData : public QSharedData {
    public:
      typedef Dissent::Connections::Id Id;

      /**
       * Default constructor for empty group
       */
      explicit GroupData(): SGPolicy(0), Size(0) {}

      explicit GroupData(const QVector<GroupContainer> &roster,
          const QHash<const Id, int> &id_to_int, const Id &leader,
          int subgroup_policy) :
        Roster(roster),
        IdtoInt(id_to_int),
        Leader(leader),
        SGPolicy(subgroup_policy),
        Size(roster.count())
      {
      }

      virtual ~GroupData() {}

      const QVector<GroupContainer> Roster;
      const QHash<const Id, int> IdtoInt;
      const Id Leader;
      const int SGPolicy;
      const int Size;

    private:
  };

  /**
   * Members of an anonymity session sorted in ascending order.
   * Contains all the components attributed to another member in the anonymity
   * group.
   */
  class Group {
    Q_GADGET
    Q_ENUMS(SubgroupPolicy);

    public:
      typedef Dissent::Crypto::AsymmetricKey AsymmetricKey;
      typedef Dissent::Connections::Id Id;
      typedef QVector<GroupContainer>::const_iterator const_iterator;

      enum SubgroupPolicy {
        CompleteGroup = 0,
        FixedSubgroup = 1,
        DisabledGroup = 255,
      };

      static QString PolicyTypeToString(SubgroupPolicy policy)
      {
        int index = staticMetaObject.indexOfEnumerator("SubgroupPolicy");
        return staticMetaObject.enumerator(index).valueToKey(policy);
      }

      static SubgroupPolicy StringToPolicyType(const QString &policy)
      {
        int index = staticMetaObject.indexOfEnumerator("SubgroupPolicy");
        int key = staticMetaObject.enumerator(index).keyToValue(policy.toUtf8().data());
        return static_cast<SubgroupPolicy>(key);
      }

      inline const_iterator begin() const { return _data->Roster.begin(); }
      inline const_iterator end() const { return _data->Roster.end(); }

      /**
       * Constructor
       * @param roster a potentially unsorted set of peers
       * @param leader the leader for the group
       * @param subgroup_policy the rules used in governing the subgroup
       */
      explicit Group(const QVector<GroupContainer> &roster,
          const Id &leader = Id::Zero(),
          SubgroupPolicy subgroup_policy = CompleteGroup);

      /**
       * Creates an empty group
       */
      explicit Group();

      /**
       * Returns the internal roster
       */
      inline const QVector<GroupContainer> &GetRoster() const { return _data->Roster; }

      /**
       * Returns the inner subgroup
       */
      const Group &GetSubgroup() const;

      /**
       * Returns the subgroup policy
       */
      inline SubgroupPolicy GetSubgroupPolicy() const { return static_cast<SubgroupPolicy>(_data->SGPolicy); }

      /**
       * Returns the leader of the group
       */
      inline const Id &GetLeader() const { return _data->Leader; }

      /**
       * Returns the Id of the peer based upon its ordered position in the group
       * @param idx the position
       */
      const Id &GetId(int idx) const;

      /**
       * Returns the Id of the peer after the specified Id
       * @param id the specified Id
       */
      const Id &Next(const Id &id) const;

      /**
       * Returns the Id of the peer before the specified Id
       * @param id the specified Id
       */
      const Id &Previous(const Id &id) const;

      /**
       * Is the specified Id a member of the Group
       * @param id the specified Id
       */
      bool Contains(const Id &id) const;

      /**
       * Returns the position of the specified Id
       * @param id the specified Id
       */
      int GetIndex(const Id &id) const;

      /**
       * Returns the key for the specified id
       * @param id the specified Id
       */
      QSharedPointer<AsymmetricKey> GetKey(const Id &id) const;

      /**
       * Returns the key for the specified index
       * @param idx the index
       */
      QSharedPointer<AsymmetricKey> GetKey(int idx) const;

      /**
       * Returns the DiffieHellman public component
       * @param id the specified id
       */
      QByteArray GetPublicDiffieHellman(const Id &id) const;

      /**
       * Returns the DiffieHellman public component
       * @param idx the specified index
       */
      QByteArray GetPublicDiffieHellman(int idx) const;

      /**
       * Returns the size of the group
       */
      int Count() const { return _data->Size; }

      /**
       * Evaluates the equality of two groups (i.e., same order, same Ids,
       * same keys, same DHs.
       */
      bool operator==(const Group &other) const;

      /**
       * Returns true if == returns false
       */
      inline bool operator!=(const Group &other) const { return !(*this == other);}

      inline static const QSharedPointer<AsymmetricKey> &EmptyKey()
      {
        static QSharedPointer<AsymmetricKey> key(new Dissent::Crypto::NullPrivateKey());
        return key;
      }
    private:
      QSharedDataPointer<GroupData> _data;
      QSharedPointer<const Group> _subgroup;
  };

  /**
   * not equals operator for group container
   * @param lhs the container used on the left hand side of the operator
   * @param rhs the container used on the right hand side of the operator
   * @returns true if the groups are not equal
   */
  inline bool operator!=(const GroupContainer &lhs, const GroupContainer &rhs) 
  {
    return (lhs.first != rhs.first) ||
          (*lhs.second != *rhs.second) ||
          (lhs.third != rhs.third);
  }

  /**
   * equals operator for group container
   * @param lhs the container used on the left hand side of the operator
   * @param rhs the container used on the right hand side of the operator
   * @returns true if the groups are equal
   */
  inline bool operator==(const GroupContainer &lhs, const GroupContainer &rhs) 
  {
    return (lhs.first == rhs.first) ||
          (*lhs.second == *rhs.second) ||
          (lhs.third == rhs.third);
  }

  /**
   * Less than operator for group container
   * @param lhs the container used on the left hand side of the operator
   * @param rhs the container used on the right hand side of the operator
   * @returns true if the lhs < rhs
   */
  inline bool operator<(const GroupContainer &lhs, const GroupContainer &rhs)
  {
    return (lhs.first < rhs.first) ||
      ((lhs.first == rhs.first) &&
       ((lhs.second->GetByteArray() < rhs.second->GetByteArray()) ||
        ((*lhs.second == *rhs.second) && (lhs.third < rhs.third))));
  }

  /**
   * Returns the whether or not the rhs is inside the lhs
   * @param set the lhs, all members in subset should be in set
   * @param subset the rhs, all members in subset should be in set
   * @returns true if all members of subset are in set
   */
  inline bool IsSubset(const Group &set, const Group &subset)
  {
    return std::includes(set.begin(), set.end(), subset.begin(), subset.end());
  }

  /**
   * Returns the set of lost members and gained members in both groups
   * @param old_group the old group roster
   * @param new_group the old group roster
   * @param lost members removed from the group
   * @param joined members new to the group
   * returns true if there was some difference
   */
  bool Difference(const Group &old_group, const Group &new_group,
      QVector<GroupContainer> &lost, QVector<GroupContainer> &gained);

  Group AddGroupMember(const Group &group, const GroupContainer &gc);

  /**
   * Returns a new group while removing the existing member for the group.
   * Group is intended to be immutable, so we just return a new group.
   */
  Group RemoveGroupMember(const Group &group, const Group::Id &id);

  /**
   * Serialize a group into a QDataStream
   */
  QDataStream &operator<<(QDataStream &stream, const Group &group);

  /**
   * Deserialize a group into a QDataStream
   */
  QDataStream &operator>>(QDataStream &stream, Group &group);
}
}

// Put these into the common namespace of Triple
namespace Dissent {
namespace Utils {
  using Dissent::Anonymity::operator==;
  using Dissent::Anonymity::operator!=;
  using Dissent::Anonymity::operator<;
}
}

#endif
