#include "Changeset.h"
#include "RCS.h"

using namespace RCS;

void Changeset::Open( File& file, const OpenFlag flags ) const
{
  file.Open( flags, m_Id );
}

void Changeset::Edit( File& file, const OpenFlag flags ) const
{
  file.Edit( flags, m_Id );
}

void Changeset::Copy( File& sourceInfo, File& targetInfo, const OpenFlag flags ) const
{
  sourceInfo.Copy( targetInfo, flags, m_Id );
}

void Changeset::Delete( File& file, const OpenFlag flags ) const
{
  file.Delete( flags, m_Id );
}

void Changeset::Create()
{
  RCS_SCOPE_TIMER( ("") );

  GetProvider()->CreateChangeset( *this );
}

void Changeset::Commit()
{
  RCS_SCOPE_TIMER( ("") );

  // if they are committing the default changeset, we need to gather
  // up the files that are open in there and move them into the newly
  // created changeset
  if ( m_Id == DefaultChangesetId )
  {
    V_File files;
    GetOpenedFiles( files );

    if ( files.size() > 0 )
    {
      Create();

      V_File::iterator fItr = files.begin();
      V_File::iterator fEnd = files.end();
      for ( ; fItr != fEnd; ++fItr )
      {
        if ( (*fItr).m_ChangesetId == DefaultChangesetId )
        {
          (*fItr).m_ChangesetId = m_Id;
          GetProvider()->Reopen( (*fItr) );
        }
      }
    }
  }

  GetProvider()->Commit( *this );
}

void Changeset::Revert( const OpenFlag flags )
{
  RCS_SCOPE_TIMER( ("") );
  GetProvider()->Revert( *this, ( ( flags & OpenFlags::UnchangedOnly ) == OpenFlags::UnchangedOnly ) );
  m_Id = DefaultChangesetId;
}

void Changeset::Reopen( File& file, const OpenFlag flags ) const
{
  RCS_SCOPE_TIMER( ("") );

  // verify we have it checked out
  file.GetInfo();
  if ( !file.IsCheckedOutByMe() )
  {
    throw Exception( "%s is not currently checked out.", file.m_LocalPath.c_str() );
  }

  file.m_ChangesetId = m_Id;

  GetProvider()->Reopen( file );
}