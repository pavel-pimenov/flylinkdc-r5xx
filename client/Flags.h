#ifndef DCPLUSPLUS_CLIENT_FLAGS_H_
#define DCPLUSPLUS_CLIENT_FLAGS_H_

class Flags
{
	public:
		typedef uint32_t MaskType;
		
		Flags() : flags(0) { }
		Flags(const Flags& rhs) : flags(rhs.flags) { }
		Flags(MaskType f) : flags(f) { }
		Flags& operator=(const Flags& rhs)
		{
			flags = rhs.flags;
			return *this;
		}
		bool isSet(MaskType aFlag) const
		{
			return (flags & aFlag) == aFlag;
		}
		bool isAnySet(MaskType aFlag) const
		{
			return (flags & aFlag) != 0;
		}
		void setFlag(MaskType aFlag)
		{
			flags |= aFlag;
		}
		void unsetFlag(MaskType aFlag)
		{
			flags &= ~aFlag;
		}
		void setFlags(MaskType aFlags)
		{
			flags = aFlags;    // !SMT!
		}
		MaskType getFlags() const
		{
			return flags;
		}
	protected:
		virtual ~Flags() { }
	private:
		MaskType flags;
};


#endif /*FLAGS_H_*/
