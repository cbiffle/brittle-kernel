#ifndef COMMON_EXCEPTIONS_H
#define COMMON_EXCEPTIONS_H

/*
 * When an IPC operation fails, the calling code receives an Exception message:
 * a message with the "error" bit set in its descriptor, whose first two data
 * words contain a 64-bit exception identifier.
 *
 * The kernel is aware of a few types of exceptions, listed below.  It may
 * learn others later.  Applications will doubtless extend this with their own
 * exceptions.  To avoid collisions, I chose not to use Unix-style dense "error
 * numbers," and instead to represent exceptions as GUIDs.
 *
 * The suggested way of manufacturing an exception number is by taking the first
 * 64 bits of the MD5 checksum of the name of the exception.  The script
 * "gen_exception.sh" in the kernel sources is intended to help with this.
 */

#include <cstdint>

enum class Exception : uint64_t {
  /*
   * The IPC operation you attempted is not valid for the given key.  This may
   * mean...
   * - That you have used a revoked key,
   * - That you have attempted to receive from an object that isn't a source
   *   of messages, or
   * - That you have sent a selector that is not recognized, or not permitted,
   *   by the recipient.
   */
  bad_operation = 0xfbfee62145593586,

  /*
   * The IPC operation you attempted would need to block in order to complete
   * successfully, but couldn't, because:
   * - You didn't set the "block" bit in the message descriptor, or
   * - You did, but the system interrupted you, e.g. due to timeout.
   */
  would_block = 0x1dfc663bc104def5,

  /*
   * You attempted to have the kernel access unprivileged memory on your
   * behalf, but the key you provided (typically a Context key) doesn't
   * actually have access to the memory in question.
   *
   * This typically occurs when trying to access a Context's registers when the
   * Context has no valid stack memory configured.
   */
  fault = 0x07b819f85884ac2e,

  /*
   * You provided an index to an operation that was invalid.
   */
  index_out_of_range = 0x0aa90fb0c1751680,

  /*
   * The syscall number given in the descriptor was not valid for this kernel.
   */
  bad_syscall = 0xabece95f618e09ea,

  /*
   * The brand you provided is not valid for use with the relevant object.
   * 
   * This typically occurs when trying to convince the Object Table to mint
   * a key.
   */
  bad_brand = 0x71decab8530ce0eb,

  /*
   * An argument (data word) you provided is not acceptable for the operation.
   */
  bad_argument = 0x05d87accc034ac82,
};

#endif  // COMMON_EXCEPTIONS_H
