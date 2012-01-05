#ifndef DISSENT_ANONYMITY_BULK_ROUND_H_GUARD
#define DISSENT_ANONYMITY_BULK_ROUND_H_GUARD

#include <QMetaEnum>
#include <QSharedPointer>

#include "Messaging/BufferSink.hpp"
#include "Messaging/GetDataCallback.hpp"
#include "Utils/Triple.hpp"

#include "Log.hpp"
#include "Round.hpp"

namespace Dissent {
namespace Crypto {
  class DiffieHellman;
}

namespace Anonymity {
  class ShuffleRound;

  /**
   * Represents a single instance of a cryptographically secure anonymous
   * exchange.
   *
   * The V1 bulk protocol consists of a shuffle round and a bulk transmission
   * phase.  The shuffle round includes an anonymous DH key and a hash for each
   * message transmitted by other peers.  The final permuted position of the DH
   * key and hash is their position or slot in the bulk message.  Using the
   * RNG, a member generates an xor mask for the slot of the anonymous sender.
   * If the member owns the slot, then they first calculate all others masks,
   * xor them together, and then xor the cleartext to arrive at their mask.
   * Each member accumulates the masks in the appropriate slot order and
   * distributes them to all other peers.  Upon accumulating all xor masks and
   * combining them via xor operations the cleartext messages are revealed.
   *
   * Bulk V1 support application broadcasting by allowing a single member (in
   * the current code, this is the leader) accumulate and aggregate all xor
   * masks and then distribute the result to all other members.  If this member
   * discovers a fault in any message, it distributes all messages to all
   * members so they can replay the round and find the faulting peer and enter
   * a shuffle phase, as done in the non-application broadcasting applicaiton.
   * If the aggregated message fails checks or there is no fault in the logs,
   * the leader has equivocated and must be removed (unimplemented). 
   * Application broadcasting is the reason for hashes of the xor masks and the
   * cleartext messages.
   */
  class BulkRound : public Round {
    Q_OBJECT

    Q_ENUMS(State);
    Q_ENUMS(MessageType);

    public:
      /**
       * A container class for the descriptors used in BulkRound
       */
      class Descriptor {
        public:
          Descriptor() : _length(-1) {}

          Descriptor(int length, const QByteArray &dh,
              const QVector<QByteArray> xor_hashes, const QByteArray &ct_hash) :
            _length(length), _dh(dh), _xor_hashes(xor_hashes), _hash(ct_hash)
          {
          }

          /**
           * Length of the message to generate
           */
          int Length() const { return _length; }

          /**
           * Anonymous DiffieHellman used to generate message
           */
          const QByteArray &PublicDh() const { return _dh; }
          
          /**
           * Hash for each xor message generated from this descriptor, matched
           * to each peers index in the group.
           */
          const QVector<QByteArray> &XorMessageHashes() const { return _xor_hashes; }

          /**
           * Hash for the cleartext message generated by this descriptor.
           */
          const QByteArray &CleartextHash() const { return _hash; }

        private:
          int _length;
          QByteArray _dh;
          QVector<QByteArray> _xor_hashes;
          QByteArray _hash;
      };

      typedef Dissent::Crypto::DiffieHellman DiffieHellman;
      typedef Dissent::Messaging::BufferSink BufferSink;
      typedef Dissent::Messaging::GetDataMethod<BulkRound> BulkGetDataCallback;
      // Descriptor index, peer index
      typedef QPair<int, int> BadHash;
      // Descriptor index, peer index, shared secret
      typedef Dissent::Utils::Triple<int, int, QByteArray> BlameEntry;

      /**
       * Varius stages of the bulk
       */
      enum State {
        Offline,
        Shuffling,
        DataSharing,
        ReceivingLeaderData,
        ProcessingLeaderData,
        Finished
      };

      /**
       * Converts a MessageType into a QString
       * @param mt value to convert
       */
      static QString StateToString(State st)
      {
        int index = staticMetaObject.indexOfEnumerator("State");
        return staticMetaObject.enumerator(index).valueToKey(st);
      }

      /**
       * Various message types sent and received
       */
      enum MessageType {
        BulkData,
        LoggedBulkData,
        AggregatedBulkData,
      };

      /**
       * Converts a MessageType into a QString
       * @param mt value to convert
       */
      static QString MessageTypeToString(MessageType mt)
      {
        int index = staticMetaObject.indexOfEnumerator("MessageType");
        return staticMetaObject.enumerator(index).valueToKey(mt);
      }

      /**
       * Constructor
       * @param group Group used during this round
       * @param creds the local nodes credentials
       * @param round_id Unique round id (nonce)
       * @param network handles message sending
       * @param get_data requests data to share during this session
       * @param create_shuffle optional parameter specifying a shuffle round
       * to create, currently used for testing
       */
      explicit BulkRound(const Group &group, const Credentials &creds,
          const Id &round_id, QSharedPointer<Network> network,
          GetDataCallback &get_data,
          CreateRound create_shuffle = &TCreateRound<ShuffleRound>);

      /**
       * Destructor
       */
      virtual ~BulkRound() {}

      /**
       * Start the bulk round
       */
      virtual bool Start();

      /**
       * Handle a data message from a remote peer
       * @param notification message from a remote peer
       */
      virtual void IncomingData(RpcRequest &notification);

      /**
       * Returns a list of members who have been blamed in the round
       */
      inline virtual const QVector<int> &GetBadMembers() const { return _bad_members; }

      /**
       * QString rep
       */
      inline virtual QString ToString() const { return "BulkRound: " + GetRoundId().ToString(); }

      /**
       * Internal bulk state
       */
      inline State GetState() { return _state; }

      /**
       * Returns the ShuffleRound used to exchange descriptors
       */
      QSharedPointer<Round> GetShuffleRound() { return _shuffle_round; }

    protected:
      /**
       * GetDataCallback into bulk data
       * @param mam the maximum amount of data to return
       * @returns a pair consisting of a qbytearray of up to max bytes and a
       * boolean true if there are more bytes to consume
       */
      virtual QPair<QByteArray, bool> GetBulkData(int max);

      /**
       * Creates and sets the descriptor given the data
       * @param cleartext data to create a descriptor for
       */
      void CreateDescriptor(const QByteArray &data);

      /**
       * If data is from a legitimate group member, it is processed
       * @param data Incoming data
       * @param id the remote peer sending the data
       */
      virtual void ProcessData(const QByteArray &data, const Id &from);

      /**
       * Does the dirty work of processing data
       * @param data Incoming data
       * @param id the remote peer sending the data
       */
      void ProcessDataBase(const QByteArray &data, const Id &from);

      /**
       * Parses through all the descriptors to generate a single transmission
       * for the bulk round, which is sent via broadcast.
       */
      void GenerateXorMessages();

      /**
       * Parses a descriptor returning the descriptor therein
       * @param data the binary descriptor form
       */
      Descriptor ParseDescriptor(const QByteArray &data);

      /**
       * Parses through an individual descriptor, setting the descriptor
       * state in the object and returns the message the descriptor
       * describes
       * @param descriptor index for message descriptor
       */
      virtual QByteArray GenerateXorMessage(int idx);

      /**
       * Returns the ShuffleSink to access serialized descriptors
       */
      const BufferSink &GetShuffleSink() const { return _shuffle_sink; }

      /**
       * Returns the parsed descriptors
       */
      const QVector<Descriptor> &GetDescriptors() const { return _descriptors; }

      /**
       * Parses and handles bulk data messages
       * @param stream serialized message
       * @param from the sender
       */
      void HandleBulkData(QDataStream &stream, const Id &from);

      /**
       * Parses and handles logged bulk data messages from the leader
       * @param stream serialized message
       * @param from the sender
       */
      void HandleLoggedBulkData(QDataStream &stream, const Id &from);

      /**
       * Parses and handles aggregated bulk data from the leader
       * @param stream serialized message
       * @param from the sender
       */
      void HandleAggregatedBulkData(QDataStream &stream, const Id &from);

      /**
       * Sets the local members descriptor for this round
       * @param my_descriptor the descriptor for this round
       */
      void SetMyDescriptor(const Descriptor &my_descriptor)
      {
        _my_descriptor = my_descriptor;
      }

      /**
       * Returns the local members descriptor for this round
       */
      const Descriptor &GetMyDescriptor() { return _my_descriptor; }

      /**
       * Sets the local members xor message for this round
       * @param my_xor_message the xor message to use in this round
       */
      void SetMyXorMessage(const QByteArray &my_xor_message)
      {
        _my_xor_message = my_xor_message;
      }

      /**
       * Return the rounds anonymous dh
       */
      const DiffieHellman *GetAnonDh() { return _anon_dh.data(); }

      /**
       * Returns if this member is the leader
       */
      inline bool IsLeader() { return _is_leader; }

    private:
      /**
       * GetDataCallback into bulk blame data
       * @param mam the maximum amount of data to return
       * @returns a pair consisting of a qbytearray of up to max bytes and a
       * boolean true if there are more bytes to consume
       */
      QPair<QByteArray, bool> GetBlameData(int max);

      /**
       * Once all bulk data messages have been received, parse them
       */
      void ProcessMessages();

      /**
       * Round finished, clean up
       */
      void Finish();
      
      /**
       * Parse the deecriptor and retrieve the cleartext bulk data
       * @param des_index index for the provided descriptor
       * @param msg_index an index into the message array
       * @returns the cleartext message
       */
      QByteArray ProcessMessage(int des_index, int msg_index);

      /**
       * Descriptor shuffle has finished and bulk has begun, prepare this
       * incase we need it.
       */
      void PrepareBlameShuffle();

      /**
       * Bulk round didn't end quite right, start the blame handling.
       */
      void BeginBlame();
      
      /**
       * Process a blame vector, sets bad members if any found
       * @param blame_vector process blame accusations
       */
      void ProcessBlame(const QVector<BlameEntry> blame_vector);

      /**
       * Enable / disable application broadcast support
       */
      bool _app_broadcast;

      /**
       * The local members anonymous index
       */
      int _my_idx;

      /**
       * Callback for creating shuffles
       */
      CreateRound _create_shuffle;

      /**
       * Holder for the GetDataCallback GetBulkData()
       */
      BulkGetDataCallback _get_bulk_data;

      /**
       * Holder for the GetDataCallback GetBlameData()
       */
      BulkGetDataCallback _get_blame_data;

      /**
       * Holds the shuffle round
       */
      QSharedPointer<Round> _shuffle_round;

      /**
       * Current state of the node
       */
      State _state;

      /**
       * Stores all validated messages that arrived before start was called
       */
      Log _offline_log;

      /**
       * Stores all validated incoming messages
       */
      Log _log;

      /**
       * Anonymous DH used to generate RNG seeds
       */
      QSharedPointer<DiffieHellman> _anon_dh;

      /**
       * Stores the output of the shuffle
       */
      BufferSink _shuffle_sink;

      /**
       * The local nodes xor message for its own message
       */
      QByteArray _my_xor_message;

      /**
       * Local nodes descriptor
       */
      Descriptor _my_descriptor;
      
      /**
       * Size determines by the accumulated length in the descriptors
       */
      int _expected_bulk_size;

      /**
       * Parsed descriptors
       */
      QVector<Descriptor> _descriptors;

      /**
       * bulk messages
       */
      QVector<QByteArray> _messages;

      /**
       * Count of received messages
       */
      int _received_messages;

      /**
       * List of bad nodes by group index
       */
      QVector<int> _bad_members;

      /**
       * List of nodes with bad hashes
       */
      QVector<BadHash> _bad_message_hash;

      /**
       * Aggregate cleartext
       */
      QVector<QByteArray> _cleartexts;

      /**
       * If is the leader for application broadcast
       */
      bool _is_leader;

    private slots:
      /**
       * Called when the descriptor shuffle ends
       */
      void ShuffleFinished();

      /**
       * Called when the blame shuffle ends
       */
      void BlameShuffleFinished();
  };

  /**
   * Xor operator for QByteArrays
   * @param dst the destination byte array
   * @param t1 lhs of the xor operation
   * @param t2 rhs of the xor operation
   */
  void Xor(QByteArray &dst, const QByteArray &t1, const QByteArray &t2);

  bool operator==(const BulkRound::Descriptor &lhs,
      const BulkRound::Descriptor &rhs);

  QDataStream &operator<<(QDataStream &stream,
      const BulkRound::Descriptor &des);

  QDataStream &operator>>(QDataStream &stream, BulkRound::Descriptor &des);
}
}

#endif
